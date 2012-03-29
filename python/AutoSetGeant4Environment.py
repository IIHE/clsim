import os
import pickle

def AutoSetGeant4Environment():
    # older versions used these hard-coded locations:
    hardCodedForGeant4_9_3and4 = \
        {"G4LEVELGAMMADATA" : os.path.expandvars("$I3_PORTS/share/geant4/data/PhotonEvaporation2.0"),
         "G4RADIOACTIVEDATA" : os.path.expandvars("$I3_PORTS/share/geant4/data/RadioactiveDecay3.2"),
         "G4LEDATA" : os.path.expandvars("$I3_PORTS/share/geant4/data/G4EMLOW6.9"),
         "G4NEUTRONHPDATA" : os.path.expandvars("$I3_PORTS/share/geant4/data/G4NDL3.13"),
         "G4ABLADATA" : os.path.expandvars("$I3_PORTS/share/geant4/data/G4ABLA3.0"),
         "G4NEUTRONXSDATA" : "(none)", # geant 4.9.3/4 does not have these
         "G4PIIDATA" : "(none)",       # geant 4.9.3/4 does not have these
         "G4REALSURFACEDATA" : "(none)"# geant 4.9.3/4 does not have these
        }
    hasOldGeant4 = (os.path.isdir(os.path.expandvars("$I3_PORTS/lib/geant4_4.9.4")) or os.path.isdir(os.path.expandvars("$I3_PORTS/lib/geant4_4.9.3"))) and (not os.path.isfile(os.path.expandvars("$I3_PORTS/bin/geant4.sh")))

    
    Geant4Variables = set(["G4ABLADATA", "G4LEDATA", "G4LEVELGAMMADATA", "G4NEUTRONHPDATA", "G4NEUTRONXSDATA", "G4PIIDATA", "G4RADIOACTIVEDATA", "G4REALSURFACEDATA"])
        
    Geant4Variables_unset = set()
    for var in Geant4Variables:
        if var not in os.environ:
            Geant4Variables_unset.add(var)
        
    Geant4Variables_set = Geant4Variables.difference(Geant4Variables_unset)
    if len(Geant4Variables_unset)>0:
        if hasOldGeant4:
            print "Not all Geant4 environment variables are set. Trying to get some of them from $I3_PORTS/bin/geant4.sh.."
        else:
            print "Not all Geant4 environment variables are set. Trying to use defaults for geant4.9.3/4.9.4.."
            
        print "already set:"
        for var in Geant4Variables_set:
            print "  *", var, "->", os.environ[var]

        print "missing:"
        for var in Geant4Variables_unset:
            print "  *", var
                
        if hasOldGeant4:
            geant4env = hardCodedForGeant4_9_3and4
        else:
            if not os.path.isfile(os.path.expandvars("$I3_PORTS/bin/geant4.sh")):
                raise RuntimeError("Cannot automatically set missing environment variables. ($I3_PORTS/bin/geant4.sh is missing.) Please set them yourself.")

            # get the environment after loading geant4.sh
            source = os.path.expandvars("source $I3_PORTS/bin/geant4.sh")
            dump = '/usr/bin/python -c "import os,pickle;print pickle.dumps(os.environ)"'
            penv = os.popen('%s && %s' %(source,dump))
            geant4env = pickle.loads(penv.read())

        print "setting from geant4.sh:"
        for var in Geant4Variables_unset:
            if var not in geant4env:
                raise RuntimeError("Cannot find the %s environment variable in the geant4.sh script." % var)
            os.environ[var] = geant4env[var]
            print "  *", var, "->", os.environ[var]

    
