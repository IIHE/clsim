/**
 * copyright (C) 2011
 * Claudio Kopper <claudio.kopper@nikhef.nl>
 * $Id$
 *
 * @file I3CLSimModule.cxx
 * @version $Revision$
 * @date $Date$
 * @author Claudio Kopper
 *
 *
 *  This file is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 *  
 */

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include "clsim/I3CLSimModule.h"

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/variant/get.hpp>
#include <boost/math/common_factor_rt.hpp>

#include "dataclasses/physics/I3MCTree.h"
#include "dataclasses/physics/I3MCTreeUtils.h"

#include "clsim/I3CLSimWlenDependentValueConstant.h"

#include "clsim/I3CLSimLightSource.h"
#include "clsim/I3CLSimLightSourceToStepConverterGeant4.h"

#include "clsim/I3CLSimModuleHelper.h"

#include <limits>
#include <set>
#include <deque>

// The module
I3_MODULE(I3CLSimModule);

I3CLSimModule::I3CLSimModule(const I3Context& context) 
: I3ConditionalModule(context),
geometryIsConfigured_(false)
{
    // define parameters
    workOnTheseStops_.clear();
#ifdef IS_Q_FRAME_ENABLED
    workOnTheseStops_.push_back(I3Frame::DAQ);
#else
    workOnTheseStops_.push_back(I3Frame::Physics);
#endif
    AddParameter("WorkOnTheseStops",
                 "Work on MCTrees found in the stream types (\"stops\") specified in this list",
                 workOnTheseStops_);

    AddParameter("RandomService",
                 "A random number generating service (derived from I3RandomService).",
                 randomService_);

    generateCherenkovPhotonsWithoutDispersion_=true;
    AddParameter("GenerateCherenkovPhotonsWithoutDispersion",
                 "The wavelength of the generated Cherenkov photons will be generated\n"
                 "according to a spectrum without dispersion. This does not change\n"
                 "the total number of photons, only the distribution of wavelengths.",
                 generateCherenkovPhotonsWithoutDispersion_);

    AddParameter("WavelengthGenerationBias",
                 "An instance of I3CLSimWlenDependentValue describing the reciprocal weight a photon gets assigned as a function of its wavelength.\n"
                 "You can set this to the wavelength depended acceptance of your DOM to pre-scale the number of generated photons.",
                 wavelengthGenerationBias_);

    AddParameter("MediumProperties",
                 "An instance of I3CLSimMediumProperties describing the ice/water properties.",
                 mediumProperties_);

    AddParameter("SpectrumTable",
                 "All spectra that could be requested by an I3CLSimStep.\n"
                 "If set to NULL/None, only spectrum #0 (Cherenkov photons) will be available.",
                 spectrumTable_);

    AddParameter("ParameterizationList",
                 "An instance I3CLSimLightSourceParameterizationSeries specifying the fast simulation parameterizations to be used.",
                 parameterizationList_);

    maxNumParallelEvents_=1000;
    AddParameter("MaxNumParallelEvents",
                 "Maximum number of events that will be processed by the GPU in parallel.",
                 maxNumParallelEvents_);

    MCTreeName_="I3MCTree";
    AddParameter("MCTreeName",
                 "Name of the I3MCTree frame object. All particles except neutrinos will be read from this tree.",
                 MCTreeName_);

    flasherPulseSeriesName_="";
    AddParameter("FlasherPulseSeriesName",
                 "Name of the I3CLSimFlasherPulseSeries frame object. Flasher pulses will be read from this object.\n"
                 "Set this to the empty string to disable flashers.",
                 flasherPulseSeriesName_);

    photonSeriesMapName_="PropagatedPhotons";
    AddParameter("PhotonSeriesMapName",
                 "Name of the I3CLSimPhotonSeriesMap frame object that will be written to the frame.",
                 photonSeriesMapName_);

    ignoreMuons_=false;
    AddParameter("IgnoreMuons",
                 "If set to True, muons will not be propagated.",
                 ignoreMuons_);

    AddParameter("OpenCLDeviceList",
                 "A vector of I3CLSimOpenCLDevice objects, describing the devices to be used for simulation.",
                 openCLDeviceList_);

    DOMRadius_=0.16510*I3Units::m; // 13 inch diameter
    AddParameter("DOMRadius",
                 "The DOM radius used during photon tracking.",
                 DOMRadius_);

    ignoreNonIceCubeOMNumbers_=false;
    AddParameter("IgnoreNonIceCubeOMNumbers",
                 "Ignore string numbers < 1 and OM numbers > 60. (AMANDA and IceTop)",
                 ignoreNonIceCubeOMNumbers_);

    geant4PhysicsListName_="QGSP_BERT";
    AddParameter("Geant4PhysicsListName",
                 "Geant4 physics list name. Examples are \"QGSP_BERT_EMV\" and \"QGSP_BERT\"",
                 geant4PhysicsListName_);

    geant4MaxBetaChangePerStep_=I3CLSimLightSourceToStepConverterGeant4::default_maxBetaChangePerStep;
    AddParameter("Geant4MaxBetaChangePerStep",
                 "Maximum change of beta=v/c per Geant4 step.",
                 geant4MaxBetaChangePerStep_);

    geant4MaxNumPhotonsPerStep_=I3CLSimLightSourceToStepConverterGeant4::default_maxNumPhotonsPerStep;
    AddParameter("Geant4MaxNumPhotonsPerStep",
                 "Approximate maximum number of Cherenkov photons generated per step by Geant4.",
                 geant4MaxNumPhotonsPerStep_);

    statisticsName_="";
    AddParameter("StatisticsName",
                 "Collect statistics in this frame object (e.g. number of photons generated or reaching the DOMs)",
                 statisticsName_);

    AddParameter("IgnoreStrings",
                 "Ignore all OMKeys with these string IDs",
                 ignoreStrings_);

    AddParameter("IgnoreDomIDs",
                 "Ignore all OMKeys with these DOM IDs",
                 ignoreDomIDs_);

    AddParameter("IgnoreSubdetectors",
                 "Ignore all OMKeys with these subdetector names",
                 ignoreSubdetectors_);

    splitGeometryIntoPartsAcordingToPosition_=false;
    AddParameter("SplitGeometryIntoPartsAcordingToPosition",
                 "If you have a geometry with multiple OMs per floor (e.g. Antares or KM3NeT-tower-like),\n"
                 "it will internally get split into subdetectors to save memory on the GPU. This is 100% transparent.\n"
                 "By default the split is according to the \"floor index\" of an OM on a floor. If you enable this\n"
                 "option, the split will also be done according to the x-y projected positions of the OMs per string.\n"
                 "This may be necessary for \"tower\" geometries.",
                 splitGeometryIntoPartsAcordingToPosition_);

    useHardcodedDeepCoreSubdetector_=false;
    AddParameter("UseHardcodedDeepCoreSubdetector",
                 "Split off DeepCore as its own two subdetectors (upper and lower part).\n"
                 "This may save constant memory on your GPU.\n"
                 "Assumes that strings [79..86] are DeepCore strings with an upper part at z>-30m\n"
                 "and a lower part at z<-30m.",
                 useHardcodedDeepCoreSubdetector_);

    
    // add an outbox
    AddOutBox("OutBox");

    frameListPhysicsFrameCounter_=0;
}

I3CLSimModule::~I3CLSimModule()
{
    log_trace("%s", __PRETTY_FUNCTION__);

    StopThread();
    
}

void I3CLSimModule::StartThread()
{
    log_trace("%s", __PRETTY_FUNCTION__);

    if (threadObj_) {
        log_debug("Thread is already running. Not starting a new one.");
        return;
    }
    
    log_trace("Thread not running. Starting a new one..");

    // clear statistics counters
    photonNumGeneratedPerParticle_.clear();
    photonWeightSumGeneratedPerParticle_.clear();
    
    // re-set flags
    threadStarted_=false;
    threadFinishedOK_=false;
    
    threadObj_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&I3CLSimModule::Thread_starter, this)));
    
    // wait for startup
    {
        boost::unique_lock<boost::mutex> guard(threadStarted_mutex_);
        for (;;)
        {
            if (threadStarted_) break;
            threadStarted_cond_.wait(guard);
        }
    }        
    
    log_trace("Thread started..");

}

void I3CLSimModule::StopThread()
{
    log_trace("%s", __PRETTY_FUNCTION__);
    
    if (threadObj_)
    {
        if (threadObj_->joinable())
        {
            log_trace("Stopping the thread..");
            threadObj_->interrupt();
            threadObj_->join(); // wait for it indefinitely
            log_trace("thread stopped.");
        } 
        else
        {
            log_trace("Thread did already finish, deleting reference.");
        }
        threadObj_.reset();
    }
}


void I3CLSimModule::Configure()
{
    log_trace("%s", __PRETTY_FUNCTION__);

    GetParameter("WorkOnTheseStops", workOnTheseStops_);
    workOnTheseStops_set_ = std::set<I3Frame::Stream>(workOnTheseStops_.begin(), workOnTheseStops_.end());
    
    GetParameter("RandomService", randomService_);
    if (!randomService_) {
        log_info("Getting the default random service from the context..");
        randomService_ = context_.Get<I3RandomServicePtr>();
        if (!randomService_) 
            log_fatal("You have to specify the \"RandomService\" parameter or add a I3RandomServiceFactor using tray.AddService()!");
    }
    
    GetParameter("GenerateCherenkovPhotonsWithoutDispersion", generateCherenkovPhotonsWithoutDispersion_);
    GetParameter("WavelengthGenerationBias", wavelengthGenerationBias_);

    GetParameter("MediumProperties", mediumProperties_);
    GetParameter("SpectrumTable", spectrumTable_);

    GetParameter("MaxNumParallelEvents", maxNumParallelEvents_);
    GetParameter("MCTreeName", MCTreeName_);
    GetParameter("FlasherPulseSeriesName", flasherPulseSeriesName_);
    GetParameter("PhotonSeriesMapName", photonSeriesMapName_);
    GetParameter("IgnoreMuons", ignoreMuons_);
    GetParameter("ParameterizationList", parameterizationList_);

    GetParameter("OpenCLDeviceList", openCLDeviceList_);

    GetParameter("DOMRadius", DOMRadius_);
    GetParameter("IgnoreNonIceCubeOMNumbers", ignoreNonIceCubeOMNumbers_);

    GetParameter("Geant4PhysicsListName", geant4PhysicsListName_);
    GetParameter("Geant4MaxBetaChangePerStep", geant4MaxBetaChangePerStep_);
    GetParameter("Geant4MaxNumPhotonsPerStep", geant4MaxNumPhotonsPerStep_);

    GetParameter("StatisticsName", statisticsName_);
    collectStatistics_ = (statisticsName_!="");
    
    GetParameter("IgnoreStrings", ignoreStrings_);
    GetParameter("IgnoreDomIDs", ignoreDomIDs_);
    GetParameter("IgnoreSubdetectors", ignoreSubdetectors_);

    GetParameter("SplitGeometryIntoPartsAcordingToPosition", splitGeometryIntoPartsAcordingToPosition_);

    GetParameter("UseHardcodedDeepCoreSubdetector", useHardcodedDeepCoreSubdetector_);

    if ((flasherPulseSeriesName_=="") && (MCTreeName_==""))
        log_fatal("You need to set at least one of the \"MCTreeName\" and \"FlasherPulseSeriesName\" parameters.");
    
    if (!wavelengthGenerationBias_) {
        wavelengthGenerationBias_ = I3CLSimWlenDependentValueConstantConstPtr(new I3CLSimWlenDependentValueConstant(1.));
    }

    if (!mediumProperties_) log_fatal("You have to specify the \"MediumProperties\" parameter!");
    if (maxNumParallelEvents_ <= 0) log_fatal("Values <= 0 are invalid for the \"MaxNumParallelEvents\" parameter!");

    if (openCLDeviceList_.empty()) log_fatal("You have to provide at least one OpenCL device using the \"OpenCLDeviceList\" parameter.");
    
    // fill wavelengthGenerators_[0] (index 0 is the Cherenkov generator)
    wavelengthGenerators_.clear();
    wavelengthGenerators_.push_back(I3CLSimModuleHelper::makeCherenkovWavelengthGenerator
                                    (wavelengthGenerationBias_,
                                     generateCherenkovPhotonsWithoutDispersion_,
                                     mediumProperties_
                                    )
                                   );
    
    if ((spectrumTable_) && (spectrumTable_->size() > 1)) {
        // a spectrum table has been configured and it contains more than the
        // default Cherenkov spectrum at index #0.
        
        for (std::size_t i=1;i<spectrumTable_->size();++i)
        {
            wavelengthGenerators_.push_back(I3CLSimModuleHelper::makeWavelengthGenerator
                                            ((*spectrumTable_)[i],
                                             wavelengthGenerationBias_,
                                             mediumProperties_
                                             )
                                            );
        }
        
        log_warn("%zu additional (non-Cherenkov) wavelength generators (spectra) have been configured.",
                 spectrumTable_->size()-1);
    }

    currentParticleCacheIndex_ = 1;
    geometryIsConfigured_ = false;
    totalSimulatedEnergyForFlush_ = 0.;
    totalNumParticlesForFlush_ = 0;
    
    if (parameterizationList_.size() > 0) {
        log_info("Using the following parameterizations:");
        
        BOOST_FOREACH(const I3CLSimLightSourceParameterization &parameterization, parameterizationList_)
        {
            I3Particle tmpParticle;
#ifndef I3PARTICLE_SUPPORTS_PDG_ENCODINGS
            tmpParticle.SetType(parameterization.forParticleType);
#else
            tmpParticle.SetPdgEncoding(parameterization.forPdgEncoding);
#endif

            log_info("  * particle=%s, from energy=%fGeV, to energy=%fGeV",
                     tmpParticle.GetTypeString().c_str(),
                     parameterization.fromEnergy/I3Units::GeV,
                     parameterization.toEnergy/I3Units::GeV);
            
        }
        
    }
    
}


/**
 * This thread takes care of passing steps from Geant4 to OpenCL
 */

bool I3CLSimModule::Thread(boost::this_thread::disable_interruption &di)
{
    // do some setup while the main thread waits..
    numBunchesSentToOpenCL_.assign(openCLStepsToPhotonsConverters_.size(), 0);
    
    // notify the main thread that everything is set up
    {
        boost::unique_lock<boost::mutex> guard(threadStarted_mutex_);
        threadStarted_=true;
    }
    threadStarted_cond_.notify_all();

    // the main thread is running again
    
    uint32_t counter=0;
    std::size_t lastDeviceIndexToUse=0;
    
    for (;;)
    {
        // retrieve steps from Geant4
        I3CLSimStepSeriesConstPtr steps;
        bool barrierWasJustReset=false;
        
        {
            boost::this_thread::restore_interruption ri(di);
            try {
                steps = geant4ParticleToStepsConverter_->GetConversionResultWithBarrierInfo(barrierWasJustReset);
            } catch(boost::thread_interrupted &i) {
                return false;
            }
        }
        
        if (!steps) 
        {
            log_debug("Got NULL I3CLSimStepSeriesConstPtr from Geant4.");
        }
        else if (steps->empty())
        {
            log_debug("Got 0 steps from Geant4, nothing to do for OpenCL.");
        }
        else
        {
            log_debug("Got %zu steps from Geant4, sending them to OpenCL",
                     steps->size());

            
            // collect statistics if requested
            if (collectStatistics_)
            {
                BOOST_FOREACH(const I3CLSimStep &step, *steps)
                {
                    const uint32_t particleID = step.identifier;

                    // skip dummy steps
                    if ((step.weight<=0.) || (step.numPhotons<=0)) continue;
                    
                    // sanity check
                    if (particleID==0) log_fatal("particleID==0, this should not happen (this index is never used)");
                    
                    (photonNumGeneratedPerParticle_.insert(std::make_pair(particleID, 0)).first->second)+=step.numPhotons;
                    (photonWeightSumGeneratedPerParticle_.insert(std::make_pair(particleID, 0.)).first->second)+=static_cast<double>(step.numPhotons)*step.weight;
                }
            }

            // determine which OpenCL device to use
            std::vector<std::size_t> fillLevels(openCLStepsToPhotonsConverters_.size());
            for (std::size_t i=0;i<openCLStepsToPhotonsConverters_.size();++i)
            {
                fillLevels[i]=openCLStepsToPhotonsConverters_[i]->QueueSize();
            }
            
            std::size_t minimumFillLevel = fillLevels[0];
            for (std::size_t i=1;i<openCLStepsToPhotonsConverters_.size();++i)
            {
                if (fillLevels[i] < minimumFillLevel)
                {
                    minimumFillLevel=fillLevels[i];
                }
            }
            
            std::size_t deviceIndexToUse=lastDeviceIndexToUse;
            do {
                ++deviceIndexToUse;
                if (deviceIndexToUse>=fillLevels.size()) deviceIndexToUse=0;
            } while (fillLevels[deviceIndexToUse] != minimumFillLevel);
            lastDeviceIndexToUse=deviceIndexToUse;
            
            // send to OpenCL
            {
                boost::this_thread::restore_interruption ri(di);
                try {
                    openCLStepsToPhotonsConverters_[deviceIndexToUse]->EnqueueSteps(steps, counter);
                } catch(boost::thread_interrupted &i) {
                    return false;
                }
            }
            
            ++numBunchesSentToOpenCL_[deviceIndexToUse];
            ++counter; // this may overflow, but it is not used for anything important/unique
        }
        
        if (barrierWasJustReset) {
            log_trace("Geant4 barrier has been reached. Exiting thread.");
            break;
        }
    }
    
    return true;
}

void I3CLSimModule::Thread_starter()
{
    // do not interrupt this thread by default
    boost::this_thread::disable_interruption di;
    
    try {
        threadFinishedOK_ = Thread(di);
        if (!threadFinishedOK_)
        {
            log_trace("thread exited due to interruption.");
        }
    } catch(...) { // any exceptions?
        log_warn("thread died unexpectedly..");
        
        throw;
    }
    
    log_debug("thread exited.");
}


void I3CLSimModule::DigestGeometry(I3FramePtr frame)
{
    log_trace("%s", __PRETTY_FUNCTION__);
    
    if (geometryIsConfigured_)
        log_fatal("This module currently supports only a single geometry per input file.");
    
    log_debug("Retrieving geometry..");
    I3GeometryConstPtr geometryObject = frame->Get<I3GeometryConstPtr>();
    if (!geometryObject) log_fatal("Geometry frame does not have an I3Geometry object!");
    
    log_debug("Converting geometry..");
    
    std::set<int> ignoreStringsSet(ignoreStrings_.begin(), ignoreStrings_.end());
    std::set<unsigned int> ignoreDomIDsSet(ignoreDomIDs_.begin(), ignoreDomIDs_.end());
    std::set<std::string> ignoreSubdetectorsSet(ignoreSubdetectors_.begin(), ignoreSubdetectors_.end());
    
    if (ignoreNonIceCubeOMNumbers_) 
    {    
        geometry_ = I3CLSimSimpleGeometryFromI3GeometryPtr
        (
         new I3CLSimSimpleGeometryFromI3Geometry(DOMRadius_, geometryObject,
                                                 ignoreStringsSet,
                                                 ignoreDomIDsSet,
                                                 ignoreSubdetectorsSet,
                                                 1,                                     // ignoreStringIDsSmallerThan
                                                 std::numeric_limits<int32_t>::max(),   // ignoreStringIDsLargerThan
                                                 1,                                     // ignoreDomIDsSmallerThan
                                                 60,                                    // ignoreDomIDsLargerThan
                                                 splitGeometryIntoPartsAcordingToPosition_,
                                                 useHardcodedDeepCoreSubdetector_)
        );
    }
    else
    {
        geometry_ = I3CLSimSimpleGeometryFromI3GeometryPtr
        (
         new I3CLSimSimpleGeometryFromI3Geometry(DOMRadius_, geometryObject,
                                                 ignoreStringsSet,
                                                 ignoreDomIDsSet,
                                                 ignoreSubdetectorsSet,
                                                 std::numeric_limits<int32_t>::min(),   // ignoreStringIDsSmallerThan
                                                 std::numeric_limits<int32_t>::max(),   // ignoreStringIDsLargerThan
                                                 std::numeric_limits<uint32_t>::min(),  // ignoreDomIDsSmallerThan
                                                 std::numeric_limits<uint32_t>::max(),  // ignoreDomIDsLargerThan
                                                 splitGeometryIntoPartsAcordingToPosition_,
                                                 useHardcodedDeepCoreSubdetector_)
        );
    }
    
    log_info("Initializing CLSim..");
    // initialize OpenCL converters
    openCLStepsToPhotonsConverters_.clear();
    
    uint64_t granularity=0;
    uint64_t maxBunchSize=0;
    
    BOOST_FOREACH(const I3CLSimOpenCLDevice &openCLdevice, openCLDeviceList_)
    {
        log_warn(" -> platform: %s device: %s",
                 openCLdevice.GetPlatformName().c_str(), openCLdevice.GetDeviceName().c_str());
        
        I3CLSimStepToPhotonConverterOpenCLPtr openCLStepsToPhotonsConverter =
        I3CLSimModuleHelper::initializeOpenCL(openCLdevice,
                                              randomService_,
                                              geometry_,
                                              mediumProperties_,
                                              wavelengthGenerationBias_,
                                              wavelengthGenerators_);
        if (!openCLStepsToPhotonsConverter)
            log_fatal("Could not initialize OpenCL!");
        
        if (openCLStepsToPhotonsConverter->GetWorkgroupSize()==0)
            log_fatal("Internal error: converter.GetWorkgroupSize()==0.");
        if (openCLStepsToPhotonsConverter->GetMaxNumWorkitems()==0)
            log_fatal("Internal error: converter.GetMaxNumWorkitems()==0.");
        
        openCLStepsToPhotonsConverters_.push_back(openCLStepsToPhotonsConverter);
        
        if (granularity==0) {
            granularity = openCLStepsToPhotonsConverter->GetWorkgroupSize();
        } else {
            // least common multiple
            const uint64_t currentGranularity = openCLStepsToPhotonsConverter->GetWorkgroupSize();
            const uint64_t newGranularity = boost::math::lcm(currentGranularity, granularity);
            
            if (newGranularity != granularity) {
                log_warn("new OpenCL device work group size is not compatible (%" PRIu64 "), changing granularity from %" PRIu64 " to %" PRIu64,
                         currentGranularity, granularity, newGranularity);
            }
            
            granularity=newGranularity;
        }

        if (maxBunchSize==0) {
            maxBunchSize = openCLStepsToPhotonsConverter->GetMaxNumWorkitems();
        } else {
            const uint64_t currentMaxBunchSize = openCLStepsToPhotonsConverter->GetMaxNumWorkitems();
            const uint64_t newMaxBunchSize = std::min(maxBunchSize, currentMaxBunchSize);
            const uint64_t newMaxBunchSizeWithGranularity = newMaxBunchSize - newMaxBunchSize%granularity;

            if (newMaxBunchSizeWithGranularity != maxBunchSize)
            {
                log_warn("maximum bunch size decreased from %" PRIu64 " to %" PRIu64 " because of new devices maximum request of %" PRIu64 " and a granularity of %" PRIu64,
                         maxBunchSize, newMaxBunchSizeWithGranularity, currentMaxBunchSize, granularity);
                
            }

            if (newMaxBunchSizeWithGranularity==0)
                log_fatal("maximum bunch sizes are incompatible with kernel work group sizes.");
            
            maxBunchSize = newMaxBunchSizeWithGranularity;
        }
        
    }
    
    
    log_info("Initializing Geant4..");
    // initialize Geant4 (will set bunch sizes according to the OpenCL settings)
    geant4ParticleToStepsConverter_ =
    I3CLSimModuleHelper::initializeGeant4(randomService_,
                                          mediumProperties_,
                                          wavelengthGenerationBias_,
                                          granularity,
                                          maxBunchSize,
                                          parameterizationList_,
                                          geant4PhysicsListName_,
                                          geant4MaxBetaChangePerStep_,
                                          geant4MaxNumPhotonsPerStep_,
                                          false); // the multiprocessor version is not yet safe to use

    
    log_info("Initialization complete.");
    geometryIsConfigured_=true;
}

namespace {
    static inline OMKey OMKeyFromOpenCLSimIDs(int16_t stringID, uint16_t domID)
    {
        return OMKey(stringID, domID);
    }
    
}

void I3CLSimModule::AddPhotonsToFrames(const I3CLSimPhotonSeries &photons)
{
    if (photonsForFrameList_.size() != frameList_.size())
        log_fatal("Internal error: cache sizes differ. (1)");
    if (photonsForFrameList_.size() != currentPhotonIdForFrame_.size())
        log_fatal("Internal error: cache sizes differ. (2)");
    
    
    BOOST_FOREACH(const I3CLSimPhoton &photon, photons)
    {
        // find identifier in particle cache
        std::map<uint32_t, particleCacheEntry>::iterator it = particleCache_.find(photon.identifier);
        if (it == particleCache_.end())
            log_fatal("Internal error: unknown particle id from OpenCL: %" PRIu32,
                      photon.identifier);
        const particleCacheEntry &cacheEntry = it->second;

        if (cacheEntry.frameListEntry >= photonsForFrameList_.size())
            log_fatal("Internal error: particle cache entry uses invalid frame cache position");
        
        //I3FramePtr &frame = frameList_[cacheEntry.frameListEntry];
        I3PhotonSeriesMap &outputPhotonMap = *(photonsForFrameList_[cacheEntry.frameListEntry]);

        // get the current photon id
        int32_t &currentPhotonId = currentPhotonIdForFrame_[cacheEntry.frameListEntry];
        
        // generate the OMKey
        const OMKey key = OMKeyFromOpenCLSimIDs(photon.stringID, photon.omID);
        
        // this either inserts a new vector or retrieves an existing one
        I3PhotonSeries &outputPhotonSeries = outputPhotonMap.insert(std::make_pair(key, I3PhotonSeries())).first->second;
        
        // append a new I3Photon to the list
        outputPhotonSeries.push_back(I3Photon());
        
        // get a reference to the new photon
        I3Photon &outputPhoton = outputPhotonSeries.back();

        // fill the photon data
        outputPhoton.SetTime(photon.GetTime());
        outputPhoton.SetID(currentPhotonId); // per-frame ID for every photon
        outputPhoton.SetWeight(photon.GetWeight());
        outputPhoton.SetParticleMinorID(cacheEntry.particleMinorID);
        outputPhoton.SetParticleMajorID(cacheEntry.particleMajorID);
        outputPhoton.SetCherenkovDist(photon.GetCherenkovDist());
        outputPhoton.SetWavelength(photon.GetWavelength());
        outputPhoton.SetGroupVelocity(photon.GetGroupVelocity());
        outputPhoton.SetNumScattered(photon.GetNumScatters());

        outputPhoton.SetPos(I3Position(photon.GetPosX(), photon.GetPosY(), photon.GetPosZ()));
        {
            I3Direction outDir;
            outDir.SetThetaPhi(photon.GetDirTheta(), photon.GetDirPhi());
            outputPhoton.SetDir(outDir);
        }

        outputPhoton.SetStartTime(photon.GetStartTime());

        outputPhoton.SetStartPos(I3Position(photon.GetStartPosX(), photon.GetStartPosY(), photon.GetStartPosZ()));
        {
            I3Direction outStartDir;
            outStartDir.SetThetaPhi(photon.GetStartDirTheta(), photon.GetStartDirPhi());
            outputPhoton.SetStartDir(outStartDir);
        }

        if (collectStatistics_)
        {
            // collect statistics
            (photonNumAtOMPerParticle_.insert(std::make_pair(photon.identifier, 0)).first->second)++;
            (photonWeightSumAtOMPerParticle_.insert(std::make_pair(photon.identifier, 0.)).first->second)+=photon.GetWeight();
        }
        
        currentPhotonId++;
    }
    
}

void I3CLSimModule::FlushFrameCache()
{
    log_debug("Flushing frame cache..");
    
    // start the connector thread if necessary
    if (!threadObj_) {
        log_trace("No thread found running during FlushFrameCache(), starting one.");
        StartThread();
    }

    // tell the Geant4 converter to not accept any new data until it is finished 
    // with its current work.
    geant4ParticleToStepsConverter_->EnqueueBarrier();
    
    // At this point the thread should keep on passing steps from Geant4 to OpenCL.
    // As soon as the barrier for Geant4 is no longer active and all data has been
    // sent to OpenCL, the thread will finish. We wait for that and then receive
    // data from the infinite OpenCL queue.
    if (!threadObj_->joinable())
        log_fatal("Thread should be joinable at this point!");
        
    log_debug("Waiting for thread..");
    threadObj_->join(); // wait for it indefinitely
    StopThread(); // stop it completely
    if (!threadFinishedOK_) log_fatal("Thread was aborted or failed.");
    log_debug("thread finished.");

    
    std::size_t totalNumOutPhotons=0;
    
    photonNumAtOMPerParticle_.clear();
    photonWeightSumAtOMPerParticle_.clear();
    
    for (std::size_t deviceIndex=0;deviceIndex<numBunchesSentToOpenCL_.size();++deviceIndex)
    {
        log_debug("Geant4 finished, retrieving results from GPU %zu..", deviceIndex);

        for (uint64_t i=0;i<numBunchesSentToOpenCL_[deviceIndex];++i)
        {
            I3CLSimStepToPhotonConverter::ConversionResult_t res =
            openCLStepsToPhotonsConverters_[deviceIndex]->GetConversionResult();
            if (!res.second) log_fatal("Internal error: received NULL photon series from OpenCL.");

            // convert to I3Photons and add to their respective frames
            AddPhotonsToFrames(*(res.second));
            
            totalNumOutPhotons += res.second->size();
        }
    }
    
    log_debug("results fetched from OpenCL.");
    
    log_debug("Got %zu photons in total during flush.", totalNumOutPhotons);

    if (collectStatistics_)
    {
        std::vector<I3CLSimEventStatisticsPtr> eventStatisticsForFrame;
        for (std::size_t i=0;i<frameList_.size();++i) {
            if (frameIsBeingWorkedOn_[i]) {
                eventStatisticsForFrame.push_back(I3CLSimEventStatisticsPtr(new I3CLSimEventStatistics()));
            } else {
                eventStatisticsForFrame.push_back(I3CLSimEventStatisticsPtr()); // NULL pointer for non-physics(/DAQ)-frames
            }
        }

        
        // generated photons (count)
        for(std::map<uint32_t, uint64_t>::const_iterator it=photonNumGeneratedPerParticle_.begin();
            it!=photonNumGeneratedPerParticle_.end();++it)
        {
            // find identifier in particle cache
            std::map<uint32_t, particleCacheEntry>::iterator it_cache = particleCache_.find(it->first);
            if (it_cache == particleCache_.end())
                log_error("Internal error: unknown particle id from Geant4: %" PRIu32,
                          it->first);
            const particleCacheEntry &cacheEntry = it_cache->second;
            
            if (cacheEntry.frameListEntry >= eventStatisticsForFrame.size())
                log_fatal("Internal error: particle cache entry uses invalid frame cache position");
            
            eventStatisticsForFrame[cacheEntry.frameListEntry]->AddNumPhotonsGeneratedWithWeights(it->second, 0.,
                                                                                                  cacheEntry.particleMajorID,
                                                                                                  cacheEntry.particleMinorID);
        }

        // generated photons (weight sum)
        for(std::map<uint32_t, double>::const_iterator it=photonWeightSumGeneratedPerParticle_.begin();
            it!=photonWeightSumGeneratedPerParticle_.end();++it)
        {
            // find identifier in particle cache
            std::map<uint32_t, particleCacheEntry>::iterator it_cache = particleCache_.find(it->first);
            if (it_cache == particleCache_.end())
                log_fatal("Internal error: unknown particle id from Geant4: %" PRIu32,
                          it->first);
            const particleCacheEntry &cacheEntry = it_cache->second;
            
            if (cacheEntry.frameListEntry >= eventStatisticsForFrame.size())
                log_fatal("Internal error: particle cache entry uses invalid frame cache position");

            eventStatisticsForFrame[cacheEntry.frameListEntry]->AddNumPhotonsGeneratedWithWeights(0, it->second,
                                                                                                  cacheEntry.particleMajorID,
                                                                                                  cacheEntry.particleMinorID);
        }

        
        // photons @ DOMs(count)
        for(std::map<uint32_t, uint64_t>::const_iterator it=photonNumAtOMPerParticle_.begin();
            it!=photonNumAtOMPerParticle_.end();++it)
        {
            // find identifier in particle cache
            std::map<uint32_t, particleCacheEntry>::iterator it_cache = particleCache_.find(it->first);
            if (it_cache == particleCache_.end())
                log_fatal("Internal error: unknown particle id from Geant4: %" PRIu32,
                          it->first);
            const particleCacheEntry &cacheEntry = it_cache->second;
            
            if (cacheEntry.frameListEntry >= eventStatisticsForFrame.size())
                log_fatal("Internal error: particle cache entry uses invalid frame cache position");
            
            eventStatisticsForFrame[cacheEntry.frameListEntry]->AddNumPhotonsAtDOMsWithWeights(it->second, 0.,
                                                                                               cacheEntry.particleMajorID,
                                                                                               cacheEntry.particleMinorID);
        }
        
        // photons @ DOMs (weight sum)
        for(std::map<uint32_t, double>::const_iterator it=photonWeightSumAtOMPerParticle_.begin();
            it!=photonWeightSumAtOMPerParticle_.end();++it)
        {
            // find identifier in particle cache
            std::map<uint32_t, particleCacheEntry>::iterator it_cache = particleCache_.find(it->first);
            if (it_cache == particleCache_.end())
                log_fatal("Internal error: unknown particle id from Geant4: %" PRIu32,
                          it->first);
            const particleCacheEntry &cacheEntry = it_cache->second;
            
            if (cacheEntry.frameListEntry >= eventStatisticsForFrame.size())
                log_fatal("Internal error: particle cache entry uses invalid frame cache position");
            
            eventStatisticsForFrame[cacheEntry.frameListEntry]->AddNumPhotonsAtDOMsWithWeights(0, it->second,
                                                                                               cacheEntry.particleMajorID,
                                                                                               cacheEntry.particleMinorID);
        }

        
        // store statistics to frame
        for (std::size_t i=0;i<frameList_.size();++i)
        {
            if (frameIsBeingWorkedOn_[i]) {
                frameList_[i]->Put(statisticsName_, eventStatisticsForFrame[i]);
            }
        }
    }    
    
    // not needed anymore
    photonWeightSumGeneratedPerParticle_.clear();
    photonNumGeneratedPerParticle_.clear();
    photonNumAtOMPerParticle_.clear();
    photonWeightSumAtOMPerParticle_.clear();

    
    log_debug("finished.");
    
    for (std::size_t identifier=0;identifier<frameList_.size();++identifier)
    {
        if (frameIsBeingWorkedOn_[identifier]) {
            log_debug("putting photons into frame %zu...", identifier);
            frameList_[identifier]->Put(photonSeriesMapName_, photonsForFrameList_[identifier]);
        }
        
        log_debug("pushing frame number %zu...", identifier);
        PushFrame(frameList_[identifier]);
    }
    
    // empty the frame cache
    particleCache_.clear();
    frameList_.clear();
    photonsForFrameList_.clear();
    currentPhotonIdForFrame_.clear();
    frameIsBeingWorkedOn_.clear();
    
    frameListPhysicsFrameCounter_=0;
}

namespace {
    bool ParticleHasMuonDaughter(const I3Particle &particle, const I3MCTree &mcTree)
    {
        const std::vector<I3Particle> daughters =
        I3MCTreeUtils::GetDaughters(mcTree, particle);

        BOOST_FOREACH(const I3Particle &daughter, daughters)
        {
            if ((daughter.GetType()==I3Particle::MuMinus) ||
                (daughter.GetType()==I3Particle::MuPlus))
                return true;
        }
        return false;
    }
    
    // version for point sources
    double DistToClosestDOM(const I3CLSimSimpleGeometry &geometry, const I3Position &pos)
    {
        double closestDist=NAN;
        
        const std::vector<double> xVect = geometry.GetPosXVector();
        const std::vector<double> yVect = geometry.GetPosYVector();
        const std::vector<double> zVect = geometry.GetPosZVector();

        
        for (std::size_t i=0;i<geometry.size();++i)
        {
            const double dx = xVect[i]-pos.GetX();
            const double dy = yVect[i]-pos.GetY();
            const double dz = zVect[i]-pos.GetZ();
            
            const double thisDist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            if (isnan(closestDist)) {
                closestDist=thisDist;
            } else {
                if (thisDist<closestDist) closestDist=thisDist;
            }
        }
        
        if (isnan(closestDist)) return 0.;
        return closestDist;
    }

    // version for tracks
    double DistToClosestDOM(const I3CLSimSimpleGeometry &geometry, const I3Position &pos, const I3Direction &dir, double length, bool nostart=false, bool nostop=false)
    {
        double closestDist=NAN;
        
        const std::vector<double> xVect = geometry.GetPosXVector();
        const std::vector<double> yVect = geometry.GetPosYVector();
        const std::vector<double> zVect = geometry.GetPosZVector();
        
        
        for (std::size_t i=0;i<geometry.size();++i)
        {
            const double Ax = xVect[i]-pos.GetX();
            const double Ay = yVect[i]-pos.GetY();
            const double Az = zVect[i]-pos.GetZ();
            
            double d_along = Ax*dir.GetX() + Ay*dir.GetY() + Az*dir.GetZ();
            
            if (!nostart) {
                if (d_along < 0.) d_along=0.;         // there is no track before its start
            }
            
            if (!nostop) {
                if (d_along > length) d_along=length; // there is no track after its end
            }
            
            const double p_along_x = pos.GetX() + dir.GetX()*d_along;
            const double p_along_y = pos.GetY() + dir.GetY()*d_along;
            const double p_along_z = pos.GetZ() + dir.GetZ()*d_along;

            // distance from point to DOM
            
            const double dx = xVect[i]-p_along_x;
            const double dy = yVect[i]-p_along_y;
            const double dz = zVect[i]-p_along_z;
            
            const double thisDist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            if (isnan(closestDist)) {
                closestDist=thisDist;
            } else {
                if (thisDist<closestDist) closestDist=thisDist;
            }
        }
        
        if (isnan(closestDist)) return 0.;
        return closestDist;
    }

}

bool I3CLSimModule::ShouldDoProcess(I3FramePtr frame)
{
    return true;
}

void I3CLSimModule::Process()
{
    I3FramePtr frame = PopFrame();
    if (!frame) return;
    
    if (frame->GetStop() == I3Frame::Geometry)
    {
        // special handling for Geometry frames
        // these will trigger a full re-initialization of OpenCL
        // (currently a second Geometry frame triggers a fatal error
        // in DigestGeometry()..)

        DigestGeometry(frame);
        PushFrame(frame);
        return;
    }
    
    // if the cache is empty and the frame stop is not Physics/DAQ, we can immediately push it
    // (and not add it to the cache)
    if ((frameList_.empty()) && (workOnTheseStops_set_.count(frame->GetStop()) == 0) )
    {
        PushFrame(frame);
        return;
    }
    
    // it's either Physics or something else..
    DigestOtherFrame(frame);
}


void I3CLSimModule::DigestOtherFrame(I3FramePtr frame)
{
    log_trace("%s", __PRETTY_FUNCTION__);
    
    frameList_.push_back(frame);
    photonsForFrameList_.push_back(I3PhotonSeriesMapPtr(new I3PhotonSeriesMap()));
    currentPhotonIdForFrame_.push_back(0);
    std::size_t currentFrameListIndex = frameList_.size()-1;


    // check if we got a geometry before starting to work
    if (!geometryIsConfigured_)
        log_fatal("Received Physics frame before Geometry frame");
    
    // start the connector thread if necessary
    if (!threadObj_) {
        log_debug("No thread found running during Physics(), starting one.");
        StartThread();
    }

    
    // a few cases where we don't work with the frame:
    
    //// not our designated Stop
    if (workOnTheseStops_set_.count(frame->GetStop()) == 0) {
        // nothing to do for this frame, it is chached, however
        frameIsBeingWorkedOn_.push_back(false); // do not touch this frame, just push it later on
        return;
    }

    // should we process it? (conditional module)
    if (!I3ConditionalModule::ShouldDoProcess(frame)) {
        frameIsBeingWorkedOn_.push_back(false); // do not touch this frame, just push it later on
        return;
    }
    
    // does it include some work?
    I3MCTreeConstPtr MCTree;
    I3CLSimFlasherPulseSeriesConstPtr flasherPulses;
    
    if (MCTreeName_ != "")
        MCTree = frame->Get<I3MCTreeConstPtr>(MCTreeName_);
    if (flasherPulseSeriesName_ != "")
        flasherPulses = frame->Get<I3CLSimFlasherPulseSeriesConstPtr>(flasherPulseSeriesName_);

    if ((!MCTree) && (!flasherPulses)) {
        // ignore frames without any MCTree and/or Flashers
        frameIsBeingWorkedOn_.push_back(false); // do not touch this frame, just push it later on
        return;
    }
    
    
    // work with this frame!
    frameIsBeingWorkedOn_.push_back(true); // this frame will receive results (->Put() will be called later)
    
    frameListPhysicsFrameCounter_++;
    
    std::deque<I3CLSimLightSource> lightSources;
    if (MCTree) ConvertMCTreeToLightSources(*MCTree, lightSources);
    
    
    
    BOOST_FOREACH(const I3CLSimLightSource &lightSource, lightSources)
    {
        if (lightSource.GetType() == I3CLSimLightSource::Particle)
        {
            const I3Particle &particle = lightSource.GetParticle();
            
            totalSimulatedEnergyForFlush_ += particle.GetEnergy();
            totalNumParticlesForFlush_++;
        }
        
        geant4ParticleToStepsConverter_->EnqueueLightSource(lightSource, currentParticleCacheIndex_);

        if (particleCache_.find(currentParticleCacheIndex_) != particleCache_.end())
            log_fatal("Internal error. Particle cache index already used.");
        
        particleCacheEntry &cacheEntry = 
        particleCache_.insert(std::make_pair(currentParticleCacheIndex_, particleCacheEntry())).first->second;
        
        cacheEntry.frameListEntry = currentFrameListIndex;
        if (lightSource.GetType() == I3CLSimLightSource::Particle) {
            cacheEntry.particleMajorID = lightSource.GetParticle().GetMajorID();
            cacheEntry.particleMinorID = lightSource.GetParticle().GetMinorID();
        } else {
            cacheEntry.particleMajorID = 0; // flashers, etc. do get ID 0,0
            cacheEntry.particleMinorID = 0;
        }
        
        // make a new index. This will eventually overflow,
        // but at that time, index 0 should be unused again.
        ++currentParticleCacheIndex_;
        if (currentParticleCacheIndex_==0) ++currentParticleCacheIndex_; // never use index==0
    }
    
    lightSources.clear();
    
    if (frameListPhysicsFrameCounter_ >= maxNumParallelEvents_)
    {
        log_debug("Flushing results for a total energy of %fGeV for %" PRIu64 " particles",
                 totalSimulatedEnergyForFlush_/I3Units::GeV, totalNumParticlesForFlush_);
                 
        totalSimulatedEnergyForFlush_=0.;
        totalNumParticlesForFlush_=0;
        
        FlushFrameCache();
        
        log_debug("============== CACHE FLUSHED ================");
    }
    
}

void I3CLSimModule::Finish()
{
    log_trace("%s", __PRETTY_FUNCTION__);
    
    log_debug("Flushing results for a total energy of %fGeV for %" PRIu64 " particles",
             totalSimulatedEnergyForFlush_/I3Units::GeV, totalNumParticlesForFlush_);

    totalSimulatedEnergyForFlush_=0.;
    totalNumParticlesForFlush_=0;
    
    FlushFrameCache();
    
    log_info("Flushing I3Tray..");
    Flush();
}




//////////////

void I3CLSimModule::ConvertMCTreeToLightSources(const I3MCTree &mcTree,
                                                std::deque<I3CLSimLightSource> &lightSources)
{
    for (I3MCTree::iterator it = mcTree.begin();
         it != mcTree.end(); ++it)
    {
        const I3Particle &particle_ref = *it;
        
        // In-ice particles only
        if (particle_ref.GetLocationType() != I3Particle::InIce) continue;
        
        // ignore particles with shape "Dark"
        if (particle_ref.GetShape() == I3Particle::Dark) continue;
        
        // check particle type
        const bool isMuon = (particle_ref.GetType() == I3Particle::MuMinus) || (particle_ref.GetType() == I3Particle::MuPlus);
        const bool isNeutrino = particle_ref.IsNeutrino();
        const bool isTrack = particle_ref.IsTrack();
        
        // mmc-icetray currently stores continuous loss entries as "unknown"
        const bool isContinuousLoss = (particle_ref.GetType() == I3Particle::ContinuousEnergyLoss) || (particle_ref.GetType() == I3Particle::unknown);
        
        // ignore continuous loss entries
        if (isContinuousLoss) {
            log_debug("ignored a continuous loss I3MCTree entry");
            continue;
        }
        
        // always ignore neutrinos
        if (isNeutrino) continue;
        
        // ignore muons if requested
        if ((ignoreMuons_) && (isMuon)) continue;
        
        if (!isTrack) 
        {
            const double distToClosestDOM = DistToClosestDOM(*geometry_, particle_ref.GetPos());
            
            if (distToClosestDOM >= 300.*I3Units::m)
            {
                log_debug("Ignored a non-track that is %fm (>300m) away from the closest DOM.",
                          distToClosestDOM);
                continue;
            }
        }
        
        // make a copy of the particle, we may need to change its length
        I3Particle particle = particle_ref;
        
        if (isTrack)
        {
            bool nostart = false;
            bool nostop = false;
            double particleLength = particle.GetLength();
            
            if (isnan(particleLength)) {
                // assume infinite track (starting at given position)
                nostop = true;
            } else if (particleLength < 0.) {
                log_warn("got track with negative length. assuming it starts at given position.");
                nostop = true;
            } else if (particleLength == 0.){
                // zero length: starting track
                nostop = true;
            }
            
            const double distToClosestDOM = DistToClosestDOM(*geometry_, particle.GetPos(), particle.GetDir(), particleLength, nostart, nostop);
            if (distToClosestDOM >= 300.*I3Units::m)
            {
                log_debug("Ignored a track that is always at least %fm (>300m) away from the closest DOM.",
                          distToClosestDOM);
                continue;
            }
        }
        
        // ignore muons with muons as child particles
        // -> those already ran through MMC(-recc) or
        // were sliced with I3MuonSlicer. Only add their
        // children.
        if (!ignoreMuons_) {
            if (ParticleHasMuonDaughter(particle_ref, mcTree)) {
                log_warn("particle has muon as daughter(s) but is not \"Dark\". Strange. Ignoring.");
                continue;
            }
        }
        
        lightSources.push_back(I3CLSimLightSource(particle));
    }
    
    
}


