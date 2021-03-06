/**
 * Copyright (c) 2011, 2012
 * Claudio Kopper <claudio.kopper@icecube.wisc.edu>
 * and the IceCube Collaboration <http://www.icecube.wisc.edu>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * $Id$
 *
 * @file TrkUserEventInformation.hh
 * @version $Revision$
 * @date $Date$
 * @author Claudio Kopper
 */

#ifndef TrkUserEventInformation_h
#define TrkUserEventInformation_h 1

#include "G4VUserEventInformation.hh"
#include "globals.hh"

#include "clsim/I3CLSimStepStore.h"
#include "clsim/I3CLSimStep.h"
#include "clsim/I3CLSimQueue.h"
#include "clsim/I3CLSimLightSourceToStepConverterGeant4.h"

#include "clsim/I3CLSimLightSource.h"
#include "clsim/I3CLSimLightSourceParameterization.h"

#include <deque>
#include <boost/tuple/tuple.hpp>

#include <boost/thread.hpp>

#include <sys/time.h>
#include <sys/resource.h>
#include "icetray/I3Units.h"

#include <map>

class TrkUserEventInformation : public G4VUserEventInformation
{
public:
    TrkUserEventInformation(uint64_t maxBunchSize_,
                            I3CLSimStepStorePtr stepStore_,
                            shared_ptr<std::deque<boost::tuple<I3CLSimLightSourceConstPtr, uint32_t, const I3CLSimLightSourceParameterization> > > sendToParameterizationQueue_,
                            const I3CLSimLightSourceParameterizationSeries &parameterizationAvailable_,
                            boost::shared_ptr<I3CLSimQueue<I3CLSimLightSourceToStepConverterGeant4::FromGeant4Pair_t> > queueFromGeant4_,
                            boost::this_thread::disable_interruption &threadDisabledInterruptionState_,
                            uint32_t currentExternalParticleID_,
                            double maxRefractiveIndex_);
    virtual ~TrkUserEventInformation();

    
    bool abortRequested;
    
    const uint64_t maxBunchSize;
    I3CLSimStepStorePtr stepStore;
    shared_ptr<std::deque<boost::tuple<I3CLSimLightSourceConstPtr, uint32_t, const I3CLSimLightSourceParameterization> > > sendToParameterizationQueue;

    const I3CLSimLightSourceParameterizationSeries &parameterizationAvailable;
    
    boost::shared_ptr<I3CLSimQueue<I3CLSimLightSourceToStepConverterGeant4::FromGeant4Pair_t> > queueFromGeant4;
    boost::this_thread::disable_interruption &threadDisabledInterruptionState;
    
    uint32_t currentExternalParticleID;
    
    double maxRefractiveIndex;
    
    struct timeval start_wallclock_, end_wallclock_;
    struct rusage start_rusage_, end_rusage_;
    
    inline void StartClock()
    {
        getrusage(RUSAGE_SELF, &start_rusage_);
        gettimeofday(&start_wallclock_, 0);
    }
    
    inline void StopClock()
    {
        getrusage (RUSAGE_SELF, &end_rusage_);
        gettimeofday (&end_wallclock_, 0);
    }
    
    inline double GetElapsedUserTime()
    {
        return delta(start_rusage_.ru_utime, end_rusage_.ru_utime);
    }

    inline double GetElapsedSysTime()
    {
        return delta(start_rusage_.ru_stime, end_rusage_.ru_stime);
    }

    inline double GetElapsedWallTime()
    {
        return delta(start_wallclock_, end_wallclock_);
    }
    
    
    inline void Print() const {};
    
    
private:
    inline double
    delta(const struct timeval &starttime, const struct timeval& stoptime)
    {
        double start = (double) starttime.tv_sec * I3Units::second + (double)starttime.tv_usec * I3Units::microsecond;
        double stop  = (double) stoptime.tv_sec * I3Units::second + (double)stoptime.tv_usec * I3Units::microsecond; 
        return stop - start;
    }
    
};

#endif
