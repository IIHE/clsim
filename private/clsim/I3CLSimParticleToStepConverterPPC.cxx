#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <cmath>

#include "clsim/I3CLSimParticleToStepConverterPPC.h"

#include "clsim/I3CLSimWlenDependentValue.h"


#include "clsim/I3CLSimParticleToStepConverterUtils.h"
using namespace I3CLSimParticleToStepConverterUtils;


const uint32_t I3CLSimParticleToStepConverterPPC::default_photonsPerStep=200;
const uint32_t I3CLSimParticleToStepConverterPPC::default_highPhotonsPerStep=0;
const double I3CLSimParticleToStepConverterPPC::default_useHighPhotonsPerStepStartingFromNumPhotons=1.0e9;



I3CLSimParticleToStepConverterPPC::I3CLSimParticleToStepConverterPPC
(I3RandomServicePtr randomService,
 uint32_t photonsPerStep,
 uint32_t highPhotonsPerStep,
 double useHighPhotonsPerStepStartingFromNumPhotons)
:
randomService_(randomService),
initialized_(false),
barrier_is_enqueued_(false),
bunchSizeGranularity_(1),
maxBunchSize_(512000),
photonsPerStep_(photonsPerStep),
highPhotonsPerStep_(highPhotonsPerStep),
useHighPhotonsPerStepStartingFromNumPhotons_(useHighPhotonsPerStepStartingFromNumPhotons)
{
    if (!randomService_)
        throw I3CLSimParticleToStepConverter_exception("No random services was provided!");
    
    if (photonsPerStep_<=0)
        throw I3CLSimParticleToStepConverter_exception("photonsPerStep may not be <= 0!");
    
    if (highPhotonsPerStep_==0) highPhotonsPerStep_=photonsPerStep_;
    
    if (highPhotonsPerStep_<photonsPerStep_)
        throw I3CLSimParticleToStepConverter_exception("highPhotonsPerStep may not be < photonsPerStep!");
    
    if (useHighPhotonsPerStepStartingFromNumPhotons_<=0.)
        throw I3CLSimParticleToStepConverter_exception("useHighPhotonsPerStepStartingFromNumPhotons may not be <= 0!");
    
}

I3CLSimParticleToStepConverterPPC::~I3CLSimParticleToStepConverterPPC()
{

}

void I3CLSimParticleToStepConverterPPC::Initialize()
{
    if (initialized_)
        throw I3CLSimParticleToStepConverter_exception("I3CLSimParticleToStepConverterPPC already initialized!");

    if (!wlenBias_)
        throw I3CLSimParticleToStepConverter_exception("WlenBias not set!");

    if (!mediumProperties_)
        throw I3CLSimParticleToStepConverter_exception("MediumProperties not set!");
    
    if (bunchSizeGranularity_ > maxBunchSize_)
        throw I3CLSimParticleToStepConverter_exception("BunchSizeGranularity must not be greater than MaxBunchSize!");

    if (maxBunchSize_%bunchSizeGranularity_ != 0)
        throw I3CLSimParticleToStepConverter_exception("MaxBunchSize is not a multiple of BunchSizeGranularity!");
    
    // make a copy of the medium properties
    {
        I3CLSimMediumPropertiesConstPtr copiedMediumProperties(new I3CLSimMediumProperties(*mediumProperties_));
        mediumProperties_ = copiedMediumProperties;
    }

    // calculate the number of photons per meter (at beta==1) for each layer
    meanPhotonsPerMeterInLayer_.clear();
    for (uint32_t i=0;i<mediumProperties_->GetLayersNum();++i)
    {
        const double nPhot = NumberOfPhotonsPerMeter(*(mediumProperties_->GetPhaseRefractiveIndex(i)),
                                                     *(wlenBias_),
                                                     mediumProperties_->GetMinWavelength(),
                                                     mediumProperties_->GetMaxWavelength());
        
        meanPhotonsPerMeterInLayer_.push_back(nPhot);
        
        log_debug("layer #%u: number of photons per meter=%f (range=[%f;%f]nm)",
                  static_cast<unsigned int>(i),
                  nPhot,
                  mediumProperties_->GetMinWavelength()/I3Units::nanometer,
                  mediumProperties_->GetMaxWavelength()/I3Units::nanometer
                  );
    }

    initialized_=true;
}


bool I3CLSimParticleToStepConverterPPC::IsInitialized() const
{
    return initialized_;
}

void I3CLSimParticleToStepConverterPPC::SetBunchSizeGranularity(uint64_t num)
{
    if (initialized_)
        throw I3CLSimParticleToStepConverter_exception("I3CLSimParticleToStepConverterPPC already initialized!");
    
    if (num<=0)
        throw I3CLSimParticleToStepConverter_exception("BunchSizeGranularity of 0 is invalid!");

    if (num!=1)
        throw I3CLSimParticleToStepConverter_exception("A BunchSizeGranularity != 1 is currently not supported!");

    bunchSizeGranularity_=num;
}

void I3CLSimParticleToStepConverterPPC::SetMaxBunchSize(uint64_t num)
{
    if (initialized_)
        throw I3CLSimParticleToStepConverter_exception("I3CLSimParticleToStepConverterPPC already initialized!");

    if (num<=0)
        throw I3CLSimParticleToStepConverter_exception("MaxBunchSize of 0 is invalid!");

    maxBunchSize_=num;
}

void I3CLSimParticleToStepConverterPPC::SetWlenBias(I3CLSimWlenDependentValueConstPtr wlenBias)
{
    if (initialized_)
        throw I3CLSimParticleToStepConverter_exception("I3CLSimParticleToStepConverterPPC already initialized!");
    
    wlenBias_=wlenBias;
}

void I3CLSimParticleToStepConverterPPC::SetMediumProperties(I3CLSimMediumPropertiesConstPtr mediumProperties)
{
    if (initialized_)
        throw I3CLSimParticleToStepConverter_exception("I3CLSimParticleToStepConverterPPC already initialized!");

    mediumProperties_=mediumProperties;
}

void I3CLSimParticleToStepConverterPPC::EnqueueParticle(const I3Particle &particle, uint32_t identifier)
{
    if (!initialized_)
        throw I3CLSimParticleToStepConverter_exception("I3CLSimParticleToStepConverterPPC is not initialized!");

    if (barrier_is_enqueued_)
        throw I3CLSimParticleToStepConverter_exception("A barrier is enqueued! You must receive all steps before enqueuing a new particle.");

    // determine current layer
    uint32_t mediumLayer =static_cast<uint32_t>(std::max(0.,(particle.GetPos().GetZ()-mediumProperties_->GetLayersZStart())/(mediumProperties_->GetLayersHeight())));
    if (mediumLayer >= mediumProperties_->GetLayersNum()) mediumLayer=mediumProperties_->GetLayersNum()-1;
    
    const double density = mediumProperties_->GetMediumDensity();
    const double meanPhotonsPerMeter = meanPhotonsPerMeterInLayer_[mediumLayer];
    //const double muonRestMass=0.105658389*I3Units.GeV;  // muon rest mass [GeV]

    log_trace("density=%fg/cm^3 in layer %u", density/(I3Units::g/I3Units::cm3), static_cast<unsigned int>(mediumLayer));
        
    
    const bool isElectron =
    (particle.GetType()==I3Particle::EMinus) ||
    (particle.GetType()==I3Particle::EPlus) ||
    (particle.GetType()==I3Particle::Brems) ||
    (particle.GetType()==I3Particle::DeltaE) ||
    (particle.GetType()==I3Particle::PairProd) ||
    (particle.GetType()==I3Particle::Gamma);

    const bool isHadron =
#ifdef I3PARTICLE_SUPPORTS_PDG_ENCODINGS
    // if we don't know it but it has a pdg code,
    // it is probably a hadron..
    (particle.GetType()==I3Particle::UnknownWithPdgEncoding) ||
#endif
    (particle.GetType()==I3Particle::Hadrons) ||
    (particle.GetType()==I3Particle::Neutron) ||
    (particle.GetType()==I3Particle::Pi0) ||
    (particle.GetType()==I3Particle::PiPlus) ||
    (particle.GetType()==I3Particle::PiMinus) ||
    (particle.GetType()==I3Particle::K0_Long) ||
    (particle.GetType()==I3Particle::KPlus) ||
    (particle.GetType()==I3Particle::KMinus) ||
    (particle.GetType()==I3Particle::PPlus) ||
    (particle.GetType()==I3Particle::PMinus) ||
    (particle.GetType()==I3Particle::K0_Short) ||
    (particle.GetType()==I3Particle::NuclInt);

    const bool isMuon =
    (particle.GetType()==I3Particle::MuMinus) ||
    (particle.GetType()==I3Particle::MuPlus);

    const double E = particle.GetEnergy()/I3Units::GeV;
    const double logE = log(E);
    const double Lrad=0.358*(I3Units::g/I3Units::cm3)/density;

    if (isElectron) {
        const double pa=2.03+0.604*logE;
        const double pb=Lrad/0.633;
        
        const double nph=5.21*(0.924*I3Units::g/I3Units::cm3)/density;
        
        const double meanNumPhotons = meanPhotonsPerMeter*nph*E;
        
        uint64_t numPhotons;
        if (meanNumPhotons > 1e7)
        {
            log_debug("HUGE EVENT: (e-m) meanNumPhotons=%f, nph=%f, E=%f (approximating possion by gaussian)", meanNumPhotons, nph, E);
            double numPhotonsDouble=0;
            do {
                numPhotonsDouble = randomService_->Gaus(meanNumPhotons, std::sqrt(meanNumPhotons));
            } while (numPhotonsDouble<0.);
            
            if (numPhotonsDouble > static_cast<double>(std::numeric_limits<uint64_t>::max()))
                log_fatal("Too many photons for counter. internal limitation.");
            numPhotons = static_cast<uint64_t>(numPhotonsDouble);
        }
        else
        {
            numPhotons = static_cast<uint64_t>(randomService_->Poisson(meanNumPhotons));
        }
        
        uint64_t usePhotonsPerStep = static_cast<uint64_t>(photonsPerStep_);
        if (static_cast<double>(numPhotons) > useHighPhotonsPerStepStartingFromNumPhotons_)
            usePhotonsPerStep = static_cast<uint64_t>(highPhotonsPerStep_);
        
        const uint64_t numSteps = numPhotons/usePhotonsPerStep;
        const uint64_t numPhotonsInLastStep = numPhotons%usePhotonsPerStep;

        log_trace("Generating %" PRIu64 " steps for electromagetic", numSteps);
        
        CascadeStepData_t cascadeStepGenInfo;
        cascadeStepGenInfo.particle=particle;
        cascadeStepGenInfo.particleIdentifier=identifier;
        cascadeStepGenInfo.photonsPerStep=usePhotonsPerStep;
        cascadeStepGenInfo.numSteps=numSteps;
        cascadeStepGenInfo.numPhotonsInLastStep=numPhotonsInLastStep;
        cascadeStepGenInfo.pa=pa;
        cascadeStepGenInfo.pb=pb;
        stepGenerationQueue_.push_back(cascadeStepGenInfo);
        
        log_trace("Generate %u steps for E=%fGeV. (electron)", static_cast<unsigned int>(numSteps+1), E);
    } else if (isHadron) {
        double pa=1.49+0.359*logE;
        double pb=Lrad/0.772;
        
        const double em=5.21*(0.924*I3Units::g/I3Units::cm3)/density;
        
        double f=1.0;
        const double E0=0.399;
        const double m=0.130;
        const double f0=0.467;
        const double rms0=0.379;
        const double gamma=1.160;
        
        double e=std::max(10.0, E);
        double F=1.-pow(e/E0, -m)*(1.-f0);
        double dF=F*rms0*pow(log10(e), -gamma);
        do {f=F+dF*randomService_->Gaus(0.,1.);} while((f<0.) || (1.<f));
        
        const double nph=f*em;
        
        const double meanNumPhotons = meanPhotonsPerMeter*nph*E;
        
        uint64_t numPhotons;
        if (meanNumPhotons > 1e7)
        {
            log_debug("HUGE EVENT: (hadron) meanNumPhotons=%f, nph=%f, E=%f (approximating possion by gaussian)", meanNumPhotons, nph, E);
            double numPhotonsDouble=0;
            do {
                numPhotonsDouble = randomService_->Gaus(meanNumPhotons, std::sqrt(meanNumPhotons));
            } while (numPhotonsDouble<0.);
            
            if (numPhotonsDouble > static_cast<double>(std::numeric_limits<uint64_t>::max()))
                log_fatal("Too many photons for counter. internal limitation.");
            numPhotons = static_cast<uint64_t>(numPhotonsDouble);
        }
        else
        {
            numPhotons = static_cast<uint64_t>(randomService_->Poisson(meanNumPhotons));
        }
        
        uint64_t usePhotonsPerStep = static_cast<uint64_t>(photonsPerStep_);
        if (static_cast<double>(numPhotons) > useHighPhotonsPerStepStartingFromNumPhotons_)
            usePhotonsPerStep = static_cast<uint64_t>(highPhotonsPerStep_);
        
        const uint64_t numSteps = numPhotons/usePhotonsPerStep;
        const uint64_t numPhotonsInLastStep = numPhotons%usePhotonsPerStep;
        
        log_trace("Generating %" PRIu64 " steps for hadron", numSteps);
        
        CascadeStepData_t cascadeStepGenInfo;
        cascadeStepGenInfo.particle=particle;
        cascadeStepGenInfo.particleIdentifier=identifier;
        cascadeStepGenInfo.photonsPerStep=usePhotonsPerStep;
        cascadeStepGenInfo.numSteps=numSteps;
        cascadeStepGenInfo.numPhotonsInLastStep=numPhotonsInLastStep;
        cascadeStepGenInfo.pa=pa;
        cascadeStepGenInfo.pb=pb;
        stepGenerationQueue_.push_back(cascadeStepGenInfo);

        log_trace("Generate %lu steps for E=%fGeV. (hadron)", static_cast<unsigned long>(numSteps+1), E);
    } else if (isMuon) {
        const double length = isnan(particle.GetLength())?(2000.*I3Units::m):(particle.GetLength());
        
        if (isnan(particle.GetLength()))
            log_warn("Muon without length found! Assigned a length of 2000m.");

        log_trace("Parameterizing muon (ID=(%" PRIu64 "/%i)) with E=%fTeV, length=%fm",
                  particle.GetMajorID(), particle.GetMinorID(),
                  E*I3Units::GeV/I3Units::TeV, length/I3Units::m);
        
        // calculation the way it's done by PPC (I hope)
        const double extr = 1. + std::max(0.0, 0.1720+0.0324*logE);
        const double muonFraction = 1./extr;
        
        const double meanNumPhotonsTotal = meanPhotonsPerMeter*(length/I3Units::m)*extr;
        
        log_trace("meanNumPhotonsTotal=%f", meanNumPhotonsTotal);
        
        const double meanNumPhotonsFromMuon = meanNumPhotonsTotal*muonFraction;
        
        uint64_t numPhotonsFromMuon;
        if (meanNumPhotonsFromMuon > 1e7)
        {
            log_debug("HUGE EVENT: (muon [   muon-like steps]) meanNumPhotons=%f, E=%f, len=%fm (approximating possion by gaussian)", meanNumPhotonsFromMuon, E, length/I3Units::m);
            double numPhotonsDouble=0;
            do {
                numPhotonsDouble = randomService_->Gaus(meanNumPhotonsFromMuon, std::sqrt(meanNumPhotonsFromMuon));
            } while (numPhotonsDouble<0.);
            
            if (numPhotonsDouble > static_cast<double>(std::numeric_limits<uint64_t>::max()))
                log_fatal("Too many photons for counter. internal limitation.");
            numPhotonsFromMuon = static_cast<uint64_t>(numPhotonsDouble);
        }
        else
        {
            numPhotonsFromMuon = static_cast<uint64_t>(randomService_->Poisson(meanNumPhotonsFromMuon));
        }
        log_trace("Generating %" PRIu64 " muon-steps for muon (mean=%f)", numPhotonsFromMuon, meanNumPhotonsFromMuon);

        const double meanNumPhotonsFromCascades = meanNumPhotonsTotal*(1.-muonFraction);

        log_trace("Generating a mean of %f cascade-steps for muon", meanNumPhotonsFromCascades);


        uint64_t numPhotonsFromCascades;
        if (meanNumPhotonsFromCascades > 1e7)
        {
            log_debug("HUGE EVENT: (muon [cascade-like steps]) meanNumPhotons=%f, E=%f, len=%fm (approximating possion by gaussian)", meanNumPhotonsFromCascades, E, length/I3Units::m);
            double numPhotonsDouble=0;
            do {
                numPhotonsDouble = randomService_->Gaus(meanNumPhotonsFromCascades, std::sqrt(meanNumPhotonsFromCascades));
            } while (numPhotonsDouble<0.);
            
            if (numPhotonsDouble > static_cast<double>(std::numeric_limits<uint64_t>::max()))
                log_fatal("Too many photons for counter. internal limitation.");
            numPhotonsFromCascades = static_cast<uint64_t>(numPhotonsDouble);
        }
        else
        {
            numPhotonsFromCascades = static_cast<uint64_t>(randomService_->Poisson(meanNumPhotonsFromCascades));
        }
        log_trace("Generating %" PRIu64 " cascade-steps for muon (mean=%f)", numPhotonsFromCascades, meanNumPhotonsFromCascades);

         
        // steps from muon
        
        uint64_t usePhotonsPerStep = static_cast<uint64_t>(photonsPerStep_);
        if (static_cast<double>(numPhotonsFromMuon) > useHighPhotonsPerStepStartingFromNumPhotons_)
            usePhotonsPerStep = static_cast<uint64_t>(highPhotonsPerStep_);
        
        const uint64_t numStepsFromMuon = numPhotonsFromMuon/usePhotonsPerStep;
        const uint64_t numPhotonsFromMuonInLastStep = numPhotonsFromMuon%usePhotonsPerStep;
        
        log_trace("Generating %" PRIu64 " steps for muon", numStepsFromMuon);

        MuonStepData_t muonStepGenInfo;
        muonStepGenInfo.particle=particle;
        muonStepGenInfo.particleIdentifier=identifier;
        muonStepGenInfo.photonsPerStep=usePhotonsPerStep;
        muonStepGenInfo.numSteps=numStepsFromMuon;
        muonStepGenInfo.numPhotonsInLastStep=numPhotonsFromMuonInLastStep;
        muonStepGenInfo.stepIsCascadeLike=false;
        muonStepGenInfo.length=length;
        stepGenerationQueue_.push_back(muonStepGenInfo);
        
        log_trace("Generate %lu steps for E=%fGeV, l=%fm. (muon[muon])", static_cast<unsigned long>((numStepsFromMuon+((numPhotonsFromMuonInLastStep>0)?1:0))), E, length/I3Units::m);
        
        
        // steps from cascade
        
        usePhotonsPerStep = static_cast<uint64_t>(photonsPerStep_);
        if (static_cast<double>(numPhotonsFromCascades) > useHighPhotonsPerStepStartingFromNumPhotons_)
            usePhotonsPerStep = static_cast<uint64_t>(highPhotonsPerStep_);

        
        const uint64_t numStepsFromCascades = numPhotonsFromCascades/usePhotonsPerStep;
        const uint64_t numPhotonsFromCascadesInLastStep = numStepsFromCascades%usePhotonsPerStep;

        //MuonStepData_t muonStepGenInfo;
        muonStepGenInfo.particle=particle;
        muonStepGenInfo.particleIdentifier=identifier;
        muonStepGenInfo.photonsPerStep=usePhotonsPerStep;
        muonStepGenInfo.numSteps=numStepsFromCascades;
        muonStepGenInfo.numPhotonsInLastStep=numPhotonsFromCascadesInLastStep;
        muonStepGenInfo.stepIsCascadeLike=true;
        muonStepGenInfo.length=length;
        stepGenerationQueue_.push_back(muonStepGenInfo);
        
        log_trace("Generate %u steps for E=%fGeV, l=%fm. (muon[cascade])", static_cast<unsigned int>((numStepsFromCascades+((numPhotonsFromCascadesInLastStep>0)?1:0))), E, length/I3Units::m);
        
    } else {
        log_fatal("I3CLSimParticleToStepConverterPPC cannot handle a %s.",
                  particle.GetTypeString().c_str());
    }
}

void I3CLSimParticleToStepConverterPPC::EnqueueBarrier()
{
    if (!initialized_)
        throw I3CLSimParticleToStepConverter_exception("I3CLSimParticleToStepConverterPPC is not initialized!");

    if (barrier_is_enqueued_)
        throw I3CLSimParticleToStepConverter_exception("A barrier is already enqueued!");

    // actually enqueue the barrier
    stepGenerationQueue_.push_back(BarrierData_t());
    barrier_is_enqueued_=true;
}

bool I3CLSimParticleToStepConverterPPC::BarrierActive() const
{
    if (!initialized_)
        throw I3CLSimParticleToStepConverter_exception("I3CLSimParticleToStepConverterPPC is not initialized!");

    return barrier_is_enqueued_;
}

bool I3CLSimParticleToStepConverterPPC::MoreStepsAvailable() const
{
    if (!initialized_)
        throw I3CLSimParticleToStepConverter_exception("I3CLSimParticleToStepConverterPPC is not initialized!");

    if (stepGenerationQueue_.size() > 0) return true;
    return false;
}

I3CLSimParticleToStepConverterPPC::MakeSteps_visitor::MakeSteps_visitor
(I3RandomService &randomService, uint64_t maxNumStepsPerStepSeries)
:randomService_(randomService),
maxNumStepsPerStepSeries_(maxNumStepsPerStepSeries)
{;}

void I3CLSimParticleToStepConverterPPC::MakeSteps_visitor::FillStep
(I3CLSimParticleToStepConverterPPC::CascadeStepData_t &data,
 I3CLSimStep &newStep,
 uint64_t photonsPerStep) const
{
    const double longitudinalPos = data.pb*I3CLSimParticleToStepConverterUtils::gammaDistributedNumber(data.pa, randomService_)*I3Units::m;
    I3CLSimParticleToStepConverterUtils::GenerateStep(newStep,
                                                      data.particle,
                                                      data.particleIdentifier,
                                                      randomService_,
                                                      photonsPerStep,
                                                      longitudinalPos);
}

void I3CLSimParticleToStepConverterPPC::MakeSteps_visitor::FillStep
(I3CLSimParticleToStepConverterPPC::MuonStepData_t &data,
 I3CLSimStep &newStep,
 uint64_t photonsPerStep) const
{
    if (data.stepIsCascadeLike) {
        const double longitudinalPos = randomService_.Uniform()*data.length;
        I3CLSimParticleToStepConverterUtils::GenerateStep(newStep,
                                                          data.particle,
                                                          data.particleIdentifier,
                                                          randomService_,
                                                          photonsPerStep,
                                                          longitudinalPos);
    } else {
        I3CLSimParticleToStepConverterUtils::GenerateStepForMuon(newStep,
                                                                 data.particle,
                                                                 data.particleIdentifier,
                                                                 photonsPerStep,
                                                                 data.length);
    }
}

template <typename T>
std::pair<I3CLSimStepSeriesConstPtr, bool>
I3CLSimParticleToStepConverterPPC::MakeSteps_visitor::operator()
(T &data) const
{
    I3CLSimStepSeriesPtr currentStepSeries(new I3CLSimStepSeries());
    
    uint64_t useNumSteps = data.numSteps;
    if (useNumSteps > maxNumStepsPerStepSeries_) useNumSteps=maxNumStepsPerStepSeries_;
    
    // make useNumSteps steps
    for (uint64_t i=0; i<useNumSteps; ++i)
    {
        currentStepSeries->push_back(I3CLSimStep());
        I3CLSimStep &newStep = currentStepSeries->back();
        FillStep(data, newStep, data.photonsPerStep);
    }
    
    // reduce the number of steps that have still to be processed
    data.numSteps -= useNumSteps;
    
    if ((data.numSteps==0) && (useNumSteps<maxNumStepsPerStepSeries_))
    {
        // make the last step (with a possible different number of photons than all the others)
        
        if (data.numPhotonsInLastStep > 0)
        {
            currentStepSeries->push_back(I3CLSimStep());
            I3CLSimStep &newStep = currentStepSeries->back();
            FillStep(data, newStep, data.numPhotonsInLastStep);
        }
        
        // we are finished with this entry, it can be removed (signal this using the return value's .second entry)
        return std::make_pair(currentStepSeries, true);
    }
    else
    {
        // we are not finished with this entry, there are more steps to come..
        return std::make_pair(currentStepSeries, false);
    }
}

// specialization for BarrierData_t
template <>
std::pair<I3CLSimStepSeriesConstPtr, bool>
I3CLSimParticleToStepConverterPPC::MakeSteps_visitor::operator()
(I3CLSimParticleToStepConverterPPC::BarrierData_t &data) const
{
    return make_pair(I3CLSimStepSeriesConstPtr(), true); // true=="entry can be removed from the queue"
}


I3CLSimStepSeriesConstPtr I3CLSimParticleToStepConverterPPC::MakeSteps(bool &barrierWasReset)
{
    barrierWasReset=false;
    
    if (stepGenerationQueue_.empty()) return I3CLSimStepSeriesConstPtr(); // queue is empty
    
    StepData_t &currentElement = stepGenerationQueue_.front();
    
    //  Let the visitor convert it into steps (the step pointer will be NULL if it is a barrier)
    std::pair<I3CLSimStepSeriesConstPtr, bool> retval =
    boost::apply_visitor(MakeSteps_visitor(*randomService_, maxBunchSize_), currentElement);
    
    I3CLSimStepSeriesConstPtr &steps = retval.first;
    const bool entryCanBeRemoved = retval.second;
    
    if (entryCanBeRemoved) stepGenerationQueue_.pop_front();
    
    // steps==NULL means a barrier was reset. Return an empty list of 
    if (!steps) {
        barrierWasReset=true;
        return I3CLSimStepSeriesConstPtr(new I3CLSimStepSeries());
    } else {
        return steps;
    }
}
    
    

I3CLSimStepSeriesConstPtr I3CLSimParticleToStepConverterPPC::GetConversionResultWithBarrierInfo(bool &barrierWasReset, double timeout)
{
    if (!initialized_)
        throw I3CLSimParticleToStepConverter_exception("I3CLSimParticleToStepConverterPPC is not initialized!");
    
    barrierWasReset=false;
    
    if (stepGenerationQueue_.empty())
    {
        throw I3CLSimParticleToStepConverter_exception("I3CLSimParticleToStepConverterPPC: no particle is enqueued!");
        return I3CLSimStepSeriesConstPtr();
    }
    
    I3CLSimStepSeriesConstPtr returnSteps = MakeSteps(barrierWasReset);
    if (!returnSteps) log_fatal("logic error. returnSteps==NULL");

    if (barrierWasReset) {
        if (barrier_is_enqueued_)
            log_error("logic error: barrier encountered, but enqueued flag is false.");
        
        barrier_is_enqueued_=false;
    }
    
    return returnSteps;
}