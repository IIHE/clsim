#ifndef TrkOpticalPhysics_hh
#define TrkOpticalPhysics_hh

#include "G4VPhysicsConstructor.hh"
#include "clsim/I3CLSimWlenDependentValue.h"

class TrkCerenkov;

class TrkOpticalPhysics : public G4VPhysicsConstructor
{
public:
    TrkOpticalPhysics(const G4String& name,
                      double maxBetaChangePerStep,
                      double maxNumPhotonsPerStep,
                      I3CLSimWlenDependentValueConstPtr wlenBias);
    virtual ~TrkOpticalPhysics();
    
    virtual void ConstructParticle();
    virtual void ConstructProcess();
    
protected:
    TrkCerenkov* theCerenkovProcess;
    
private:
    double maxBetaChangePerStep_;
    double maxNumPhotonsPerStep_;
    I3CLSimWlenDependentValueConstPtr wlenBias_;
};

#endif
