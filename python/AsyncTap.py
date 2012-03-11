import multiprocessing
import signal
import sys

from icecube import icetray, dataclasses
from I3Tray import *


# run a writer tray, getting frames from a queue (this runs as a subprocess)
def RunAsyncTray(queue,segment,segmentArgs,debug):
    # just pushes frame on a queue
    class AsyncReceiver(icetray.I3Module):
        def __init__(self, context):
            icetray.I3Module.__init__(self, context)
            self.AddParameter("Debug", "Output some status information for debugging", False)
            self.AddParameter("Queue", "", None)
            self.AddOutBox("OutBox")
            self.nframes = 0
        def Configure(self):
            self.debug = self.GetParameter("Debug")
            self.queue = self.GetParameter("Queue")
        def Process(self):
            #print "waiting for frame..", self.nframes
            #sys.stdout.flush()
            frame = self.queue.get()
            #if frame is not None:
            #    #print "frame received", self.nframes , frame.Stop
            #    #sys.stdout.flush()
        
            if frame is None:
                if self.debug: 
                    print "requesting suspension"
                    sys.stdout.flush()
                self.RequestSuspension()
            else:
                self.nframes += 1
                self.PushFrame(frame)
        def Finish(self):
            if self.debug:
                print "received", self.nframes, "frames"
                sys.stdout.flush()
    
    class RerouteSigIntToDefault(icetray.I3Module):
        def __init__(self, context):
            icetray.I3Module.__init__(self, context)
            self.AddOutBox("OutBox")
        def Configure(self):
            # Reroute ctrl+C (sigint) to the default handler.
            # (This needs to be done in a module, as IceTray re-routes
            # the signal in tray.Execute())
            signal.signal(signal.SIGINT, signal.SIG_IGN)
        def DAQ(self, frame):
            self.PushFrame(frame)
    
    writeTray = I3Tray()
    writeTray.AddModule(AsyncReceiver, "theAsyncReceiver",
        Queue=queue, Debug=debug)
    writeTray.AddModule(RerouteSigIntToDefault, "rerouteSigIntToDefault")
    
    if hasattr(segment, '__i3traysegment__'):
        writeTray.AddSegment(segment,"theSegment", **segmentArgs)
    else:
        writeTray.AddModule(segment,"theModule", **segmentArgs)
    
    writeTray.AddModule("TrashCan", "theCan")

    if debug: print "worker starting.."
    writeTray.Execute()
    writeTray.Finish()
    if debug: print "worker finished."



class AsyncTap(icetray.I3ConditionalModule):
    """
    Starts a module or a segment on its own tray in its own process
    and pipes frames from the current tray to the child tray.
    Can be used for things like asynchronous file writing.
    The frames will never get back to the master module,
    so this is effecively a "fork".
    """

    def __init__(self, context):
        icetray.I3ConditionalModule.__init__(self, context)
        self.AddParameter("Debug", "Output some status information for debugging", False)
        self.AddParameter("BufferSize", "How many frames should be buffered", 2000)
        self.AddParameter("Segment", "The tray segment to run asynchronously", None)
        self.AddParameter("Args", "A dictionary with keyword arguments for the asynchronous segment.", dict())
        self.AddOutBox("OutBox")
        self.nframes = 0
        self.suspensionRequested=False

    def CheckOnChildProcess(self):
        if self.suspensionRequested: return False
        
        # check to see if child process is still alive
        if not self.process.is_alive():
            print "ERROR: ****** child tray died unexpectedly. Terminating master tray. ******"
            self.RequestSuspension()
            self.suspensionRequested=True
            return False
        else:
            return True

    def Configure(self):
        self.debug = self.GetParameter("Debug")
        self.buffersize = self.GetParameter("BufferSize")
        self.segment = self.GetParameter("Segment")
        self.args = self.GetParameter("Args")

        if self.debug: print "starting child process.."
        self.queueToProcess = multiprocessing.Queue(self.buffersize)
        self.process = multiprocessing.Process(target=RunAsyncTray, args=(self.queueToProcess,self.segment,self.args,self.debug,))
        self.process.start()
        if self.debug: print "child process running."
        
        self.CheckOnChildProcess()

    def Process(self):
        frame = self.PopFrame()
        #print "sending frame..", self.nframes , frame.Stop
        #sys.stdout.flush()
        if self.CheckOnChildProcess():
            self.queueToProcess.put(frame)
        #print "frame sent", self.nframes , frame.Stop
        #sys.stdout.flush()
        self.nframes += 1
        self.PushFrame(frame)

    def Finish(self):
        if self.debug: print "sent", self.nframes, "frames"
        if self.CheckOnChildProcess():
            self.queueToProcess.put(None) # signal receiver to quit
        
        if self.debug: print "async module finished. waiting for child tray.."
        self.process.join()
        if self.debug: print "child tray finished."

