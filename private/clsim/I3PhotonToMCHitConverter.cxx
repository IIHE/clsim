/**
 * copyright (C) 2011
 * Claudio Kopper <claudio.kopper@nikhef.nl>
 * $Id$
 *
 * @file I3PhotonToMCHitConverter.cxx
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

#include <algorithm>

#include "clsim/I3PhotonToMCHitConverter.h"

#include <boost/foreach.hpp>

#include "clsim/I3Photon.h"

#include "dataclasses/geometry/I3Geometry.h"
#include "dataclasses/physics/I3MCHit.h"
#include "dataclasses/physics/I3MCTree.h"

#include "dataclasses/I3Constants.h"

// The module
I3_MODULE(I3PhotonToMCHitConverter);

I3PhotonToMCHitConverter::I3PhotonToMCHitConverter(const I3Context& context) 
: I3ConditionalModule(context)
{
    AddParameter("RandomService",
                 "A random number generating service (derived from I3RandomService).",
                 randomService_);

    inputPhotonSeriesMapName_="PropagatedPhotons";
    AddParameter("InputPhotonSeriesMapName",
                 "Name of the input I3PhotonSeriesMap frame object.",
                 inputPhotonSeriesMapName_);

    outputMCHitSeriesMapName_="MCHitSeriesMap";
    AddParameter("OutputMCHitSeriesMapName",
                 "Name of the output I3MCHitSeries frame object.",
                 outputMCHitSeriesMapName_);

    MCTreeName_="I3MCTree";
    AddParameter("MCTreeName",
                 "Name of the I3MCTree frame object. All photon particle IDs are checked against this tree.",
                 MCTreeName_);

    AddParameter("WavelengthAcceptance",
                 "Wavelength acceptance of the (D)OM as a I3WlenDependedValue object.",
                 wavelengthAcceptance_);

    AddParameter("AngularAcceptance",
                 "Angular acceptance of the (D)OM as a I3WlenDependedValue object.",
                 angularAcceptance_);

    DOMOversizeFactor_=1.;
    AddParameter("DOMOversizeFactor",
                 "Specifiy the \"oversize factor\" (i.e. DOM radius scaling factor) you used during the CLSim run.\n"
                 "The photon arrival times will be corrected. In practice this means your large spherical DOMs will\n"
                 "become ellipsoids.",
                 DOMOversizeFactor_);

    DOMRadiusWithoutOversize_=0.16510*I3Units::m; // 13 inch diameter
    AddParameter("DOMRadiusWithoutOversize",
                 "Specifiy the DOM radius. Do not include oversize factors here.",
                 DOMRadiusWithoutOversize_);

    AddParameter("PMTPhotonSimulator",
                 "Optional after-pulse, late-pulse and jitter simulator object,\n"
                 "an instance of I3CLSimPMTPhotonSimulator",
                 pmtPhotonSimulator_);

    
    defaultRelativeDOMEfficiency_=1.;
    AddParameter("DefaultRelativeDOMEfficiency",
                 "Default relative efficiency. This value is used if no entry is available from I3Calibration.",
                 defaultRelativeDOMEfficiency_);

    replaceRelativeDOMEfficiencyWithDefault_=false;
    AddParameter("ReplaceRelativeDOMEfficiencyWithDefault",
                 "Always use the default relative efficiency, ignore other values from I3Calibration.",
                 replaceRelativeDOMEfficiencyWithDefault_);

    ignoreDOMsWithoutDetectorStatusEntry_=false;
    AddParameter("IgnoreDOMsWithoutDetectorStatusEntry",
                 "Do not generate hits for OMKeys not found in the I3DetectorStatus.I3DOMStatusMap",
                 ignoreDOMsWithoutDetectorStatusEntry_);

    onlyWarnAboutInvalidPhotonPositions_=false;
    AddParameter("OnlyWarnAboutInvalidPhotonPositions",
                 "Make photon position/radius check a warning only (instead of a fatal condition)",
                 onlyWarnAboutInvalidPhotonPositions_);

    // add an outbox
    AddOutBox("OutBox");

}

I3PhotonToMCHitConverter::~I3PhotonToMCHitConverter()
{
    log_trace("%s", __PRETTY_FUNCTION__);
}


void I3PhotonToMCHitConverter::Configure()
{
    log_trace("%s", __PRETTY_FUNCTION__);

    GetParameter("RandomService", randomService_);

    GetParameter("InputPhotonSeriesMapName", inputPhotonSeriesMapName_);
    GetParameter("OutputMCHitSeriesMapName", outputMCHitSeriesMapName_);

    GetParameter("MCTreeName", MCTreeName_);

    GetParameter("WavelengthAcceptance", wavelengthAcceptance_);
    GetParameter("AngularAcceptance", angularAcceptance_);

    GetParameter("DOMOversizeFactor", DOMOversizeFactor_);
    GetParameter("DOMRadiusWithoutOversize", DOMRadiusWithoutOversize_);

    GetParameter("PMTPhotonSimulator", pmtPhotonSimulator_);
    GetParameter("DefaultRelativeDOMEfficiency", defaultRelativeDOMEfficiency_);
    GetParameter("ReplaceRelativeDOMEfficiencyWithDefault", replaceRelativeDOMEfficiencyWithDefault_);
    GetParameter("IgnoreDOMsWithoutDetectorStatusEntry", ignoreDOMsWithoutDetectorStatusEntry_);

    GetParameter("OnlyWarnAboutInvalidPhotonPositions", onlyWarnAboutInvalidPhotonPositions_);

    if (replaceRelativeDOMEfficiencyWithDefault_)
    {
        if (std::isnan(defaultRelativeDOMEfficiency_))
            log_fatal("You need to set \"DefaultRelativeDOMEfficiency\" to a value other than NaN if you enabled \"ReplaceRelativeDOMEfficiencyWithDefault\"");
    }

    if (defaultRelativeDOMEfficiency_<0.) 
        log_fatal("The \"DefaultRelativeDOMEfficiency\" parameter must not be < 0!");
    
    if (!wavelengthAcceptance_)
        log_fatal("The \"WavelengthAcceptance\" parameter must not be empty.");
    if (!angularAcceptance_)
        log_fatal("The \"AngularAcceptance\" parameter must not be empty.");
    
    if (!wavelengthAcceptance_->HasNativeImplementation())
        log_fatal("The wavelength acceptance function must have a native (i.e. non-OpenCL) implementation!");
    if (!angularAcceptance_->HasNativeImplementation())
        log_fatal("The angular acceptance function must have a native (i.e. non-OpenCL) implementation!");
    
    
    if (!randomService_) {
        log_info("No random service provided as a parameter, trying to get one from the context..");
        randomService_ = context_.Get<I3RandomServicePtr>();
    }
    
    if (!randomService_)
        log_fatal("No random service provided using the \"RandomService\" parameter (and there is none installed on the context).");
    
    if (pmtPhotonSimulator_)
        pmtPhotonSimulator_->SetRandomService(randomService_);

}

void I3PhotonToMCHitConverter::DetectorStatus(I3FramePtr frame)
{
    log_trace("%s", __PRETTY_FUNCTION__);
        
    I3DetectorStatusConstPtr detectorStatus = frame->Get<I3DetectorStatusConstPtr>("I3DetectorStatus");
    if (!detectorStatus)
        log_fatal("detector status frame does not have an I3DetectorStatus entry");
    
    if (pmtPhotonSimulator_)
        pmtPhotonSimulator_->SetDetectorStatus(detectorStatus);

    // store it for later
    status_ = detectorStatus;

    PushFrame(frame);
}

void I3PhotonToMCHitConverter::Calibration(I3FramePtr frame)
{
    log_trace("%s", __PRETTY_FUNCTION__);
    
    I3CalibrationConstPtr calibration = frame->Get<I3CalibrationConstPtr>("I3Calibration");
    if (!calibration)
        log_fatal("calibration frame does not have an I3Calibration entry");
    
    if (pmtPhotonSimulator_)
        pmtPhotonSimulator_->SetCalibration(calibration);

    // store it for later
    calibration_ = calibration;
    
    PushFrame(frame);
}

namespace {
    // Return whether first element is greater than the second
    bool MCHitTimeLess(const I3MCHit &elem1, const I3MCHit &elem2)
    {
        return elem1.GetTime() < elem2.GetTime();
    }
}

#ifdef IS_Q_FRAME_ENABLED
void I3PhotonToMCHitConverter::DAQ(I3FramePtr frame)
#else
void I3PhotonToMCHitConverter::Physics(I3FramePtr frame)
#endif
{
    log_trace("%s", __PRETTY_FUNCTION__);
    
    // First we need to get our geometry
	const I3Geometry& geometry = frame->Get<I3Geometry>();

    if (!calibration_)
        log_fatal("no Calibration frame yet, but received a Physics frame.");

    if ((!status_) && (ignoreDOMsWithoutDetectorStatusEntry_))
        log_fatal("no DetectorStatus frame yet, but received a Physics frame.");
    
    I3PhotonSeriesMapConstPtr inputPhotonSeriesMap = frame->Get<I3PhotonSeriesMapConstPtr>(inputPhotonSeriesMapName_);
    if (!inputPhotonSeriesMap) {
        log_debug("Frame does not contain an I3PhotonSeriesMap named \"%s\".",
                  inputPhotonSeriesMapName_.c_str());
        
        // do nothing if there is no input data
        PushFrame(frame);
        return;
    }
    
    // currently, the only reason we need the MCTree is that I3MCHit does
    // only allow setting the major/minor particle IDs using an existing
    // I3Particle instance with that ID combination.
    I3MCTreeConstPtr MCTree = frame->Get<I3MCTreeConstPtr>(MCTreeName_);

    // allocate the output hitSeriesMap
    I3MCHitSeriesMapPtr outputMCHitSeriesMap(new I3MCHitSeriesMap());
    
    std::map<std::pair<uint64_t, int>, const I3Particle *> mcTreeIndex;
    if (MCTree) {
        // build an index into the I3MCTree
        for (I3MCTree::iterator it = MCTree->begin();
             it != MCTree->end(); ++it)
        {
            const I3Particle &particle = *it;
            mcTreeIndex.insert(std::make_pair(std::make_pair(particle.GetMajorID(), particle.GetMinorID()), &particle));
        }
    }    
    
#ifndef HAS_MULTIPMT_SUPPORT    
    // DOM is looking downwards
    const double DOMDir_x = 0.;
    const double DOMDir_y = 0.;
    const double DOMDir_z = -1.;
#endif
    
    BOOST_FOREACH(const I3PhotonSeriesMap::value_type &it, *inputPhotonSeriesMap)
    {
        const OMKey &key = it.first;
        const I3PhotonSeries &photons = it.second;

        if (ignoreDOMsWithoutDetectorStatusEntry_) {
            std::map<OMKey, I3DOMStatus>::const_iterator om_stat = status_->domStatus.find(key);
            if (om_stat==status_->domStatus.end()) continue; // ignore it
            if (om_stat->second.pmtHV==0.) continue; // ignore pmtHV==0
        }
        
        // Find the current OM in the geometry map
		I3OMGeoMap::const_iterator geo_it = geometry.omgeo.find(key);
		if (geo_it == geometry.omgeo.end())
			log_fatal("OM (%i/%u) not found in the current geometry map!", key.GetString(), key.GetOM());
		const I3OMGeo &om = geo_it->second;

#ifdef HAS_MULTIPMT_SUPPORT    
        // get DOM (PMT) direction from geometry
        const I3OMTypeInfo &omTypeInfo = geometry.GetOMTypeInfo(key);
        if (omTypeInfo.GetNumPMTs() != 1) {
            log_fatal("This module does only support DOMs with a single PMT. numPMTs=%u",
                      omTypeInfo.GetNumPMTs());
        }
        I3Direction pmtDir = geometry.GetPMTDir(key, 0); // pmtNum==0

        const double DOMDir_x = pmtDir.GetX();
        const double DOMDir_y = pmtDir.GetY();
        const double DOMDir_z = pmtDir.GetZ();
#endif
        
        // Find the current OM in the calibration map
        
        // relative DOM efficiency from calibration
        double efficiency_from_calibration=NAN;

        if (replaceRelativeDOMEfficiencyWithDefault_)
        {
            efficiency_from_calibration=defaultRelativeDOMEfficiency_;
        }
        else 
        {
            if (!calibration_) {
                if (isnan(defaultRelativeDOMEfficiency_)) {
                    log_fatal("There is no valid calibration! (Consider setting \"DefaultRelativeDOMEfficiency\" != NaN)");
                } else {
                    efficiency_from_calibration = defaultRelativeDOMEfficiency_;
                    log_debug("OM (%i/%u): efficiency_from_calibration=%g (default (I3Calibration not found))",
                              key.GetString(), key.GetOM(),
                              efficiency_from_calibration);
                }
            } else {
                std::map<OMKey, I3DOMCalibration>::const_iterator cal_it = calibration_->domCal.find(key);
                if (cal_it == calibration_->domCal.end()) {
                    if (isnan(defaultRelativeDOMEfficiency_)) {
                        log_fatal("OM (%i/%u) not found in the current calibration map! (Consider setting \"DefaultRelativeDOMEfficiency\" != NaN)", key.GetString(), key.GetOM());
                    } else {
                        efficiency_from_calibration = defaultRelativeDOMEfficiency_;
                        log_debug("OM (%i/%u): efficiency_from_calibration=%g (default (no calib))",
                                  key.GetString(), key.GetOM(),
                                  efficiency_from_calibration);
                    }
                } else {
                    const I3DOMCalibration &domCalibration = cal_it->second;
                    efficiency_from_calibration=domCalibration.GetRelativeDomEff();
                    
                    if (isnan(efficiency_from_calibration)) {
                        if (isnan(defaultRelativeDOMEfficiency_)) {
                            log_fatal("OM (%i/%u) found in the current calibration map, but it is NaN! (Consider setting \"DefaultRelativeDOMEfficiency\" != NaN)", key.GetString(), key.GetOM());
                        } else {                
                            efficiency_from_calibration = defaultRelativeDOMEfficiency_;
                            log_debug("OM (%i/%u): efficiency_from_calibration=%g (default (was: NaN))",
                                      key.GetString(), key.GetOM(),
                                      efficiency_from_calibration);
                        }
                    } else {
                        log_debug("OM (%i/%u): efficiency_from_calibration=%g",
                                  key.GetString(), key.GetOM(),
                                  efficiency_from_calibration);
                    }
                }
            }
        }
        
        // a pointer to the output vector. The vector will be allocated 
        // by the map, this is merely a pointer to it in case we have multiple
        // hits per OM.
        I3MCHitSeries *hits = NULL;

        BOOST_FOREACH(const I3Photon &photon, photons)
        {
            double hitProbability = photon.GetWeight();
            if (hitProbability < 0.) log_fatal("Photon with negative weight found.");
            if (hitProbability == 0.) continue;

            const double dx=photon.GetDir().GetX();
            const double dy=photon.GetDir().GetY();
            const double dz=photon.GetDir().GetZ();
            const double px=om.position.GetX()-photon.GetPos().GetX();
            const double py=om.position.GetY()-photon.GetPos().GetY();
            const double pz=om.position.GetZ()-photon.GetPos().GetZ();
            const double pr2 = px*px + py*py + pz*pz;
            
            double photonCosAngle = -(dx * DOMDir_x +
                                      dy * DOMDir_y +
                                      dz * DOMDir_z);
            photonCosAngle = std::max(-1., std::min(1., photonCosAngle));
            
            // sanity check: are photons on the OM's surface?
            const double distFromDOMCenter = std::sqrt(pr2);
            if (std::abs(distFromDOMCenter - DOMOversizeFactor_*DOMRadiusWithoutOversize_) > 3.*I3Units::cm) {
                if (onlyWarnAboutInvalidPhotonPositions_) {
                    log_warn("distance not %f*%f=%fmm.. it is %fmm (diff=%gmm) (OMKey=(%i,%u) (photon @ pos=(%g,%g,%g)m) (DOM @ pos=(%g,%g,%g)m)",
                             DOMOversizeFactor_,
                             DOMRadiusWithoutOversize_/I3Units::mm,
                             DOMOversizeFactor_*DOMRadiusWithoutOversize_/I3Units::mm,
                             distFromDOMCenter/I3Units::mm,
                             (distFromDOMCenter-DOMOversizeFactor_*DOMRadiusWithoutOversize_)/I3Units::mm,
                             key.GetString(), key.GetOM(),
                             photon.GetPos().GetX()/I3Units::m,
                             photon.GetPos().GetY()/I3Units::m,
                             photon.GetPos().GetZ()/I3Units::m,
                             om.position.GetX()/I3Units::m,
                             om.position.GetY()/I3Units::m,
                             om.position.GetZ()/I3Units::m
                             );
                } else {
                    log_fatal("distance not %f*%f=%fmm.. it is %fmm (diff=%gmm) (OMKey=(%i,%u) (photon @ pos=(%g,%g,%g)m) (DOM @ pos=(%g,%g,%g)m)",
                              DOMOversizeFactor_,
                              DOMRadiusWithoutOversize_/I3Units::mm,
                              DOMOversizeFactor_*DOMRadiusWithoutOversize_/I3Units::mm,
                              distFromDOMCenter/I3Units::mm,
                              (distFromDOMCenter-DOMOversizeFactor_*DOMRadiusWithoutOversize_)/I3Units::mm,
                              key.GetString(), key.GetOM(),
                              photon.GetPos().GetX()/I3Units::m,
                              photon.GetPos().GetY()/I3Units::m,
                              photon.GetPos().GetZ()/I3Units::m,
                              om.position.GetX()/I3Units::m,
                              om.position.GetY()/I3Units::m,
                              om.position.GetZ()/I3Units::m
                              );
                }
            }
            
            // sanity check for unscattered photons: is their direction ok
            // w.r.t. the vector from emission to detection?
            if (photon.GetNumScattered()==0)
            {
                double ppx = photon.GetPos().GetX()-photon.GetStartPos().GetX();
                double ppy = photon.GetPos().GetY()-photon.GetStartPos().GetY();
                double ppz = photon.GetPos().GetZ()-photon.GetStartPos().GetZ();
                const double ppl = std::sqrt(ppx*ppx + ppy*ppy + ppz*ppz);
                ppx/=ppl; ppy/=ppl; ppz/=ppl;
                const double cosang = dx*ppx + dy*ppy + dz*ppz;
                
                if ((cosang < 0.9) && (ppl>1.*I3Units::m)) {
                    log_fatal("unscattered photon direction is inconsistent: cos(ang)==%f, d=(%f,%f,%f), pp=(%f,%f,%f) pp_l=%f",
                              cosang,
                              dx, dy, dz,
                              ppx, ppy, ppz,
                              ppl
                              );
                }
            }
            
#ifndef I3_OPTIMIZE
            const double photonAngle = std::acos(photonCosAngle);
            log_trace("Photon (lambda=%fnm, angle=%fdeg, dist=%fm) has weight %g",
                     photon.GetWavelength()/I3Units::nanometer,
                     photonAngle/I3Units::deg,
                     distFromDOMCenter/I3Units::m,
                     hitProbability);
#endif
            
            hitProbability *= wavelengthAcceptance_->GetValue(photon.GetWavelength());
            log_trace("After wlen acceptance: prob=%g (wlen acceptance is %f)",
                     hitProbability, wavelengthAcceptance_->GetValue(photon.GetWavelength()));

            hitProbability *= angularAcceptance_->GetValue(photonCosAngle);
            log_trace("After wlen&angular acceptance: prob=%g (angular acceptance is %f)",
                      hitProbability, angularAcceptance_->GetValue(photonCosAngle));

            hitProbability *= efficiency_from_calibration;
            log_trace("After efficiency from calibration: prob=%g (efficiency_from_calibration=%f)",
                      hitProbability, efficiency_from_calibration);

            if (hitProbability > 1.)
                log_fatal("hitProbability > 1: cannot continue. your hit weights are too high.");
            
            // does it survive?
            if (hitProbability <= randomService_->Uniform()) continue;

            // find the particle
            const I3Particle *particle = NULL;
            
            if ((photon.GetParticleMajorID() != 0) || (photon.GetParticleMinorID() != 0))
            {
                // index (0,0) is used for flasher photons, set no hit particle for those
                std::map<std::pair<uint64_t, int>, const I3Particle *>::const_iterator it = 
                mcTreeIndex.find(std::make_pair(photon.GetParticleMajorID(), photon.GetParticleMinorID()));
                if (it==mcTreeIndex.end())
                    log_fatal("Particle with id maj=%" PRIu64 ", min=%i does not exist in MC tree, but we have a photon that claims it was created by that particle..",
                              photon.GetParticleMajorID(), photon.GetParticleMinorID());
                particle = it->second;
            }
        
            // allocate the output vector if not already done
            if (!hits) hits = &(outputMCHitSeriesMap->insert(std::make_pair(key, I3MCHitSeries())).first->second);

            // correct timing for oversized DOMs
            double correctedTime = photon.GetTime();
            {
                const double dot = px*dx + py*dy + pz*dz;
                const double bringForward = dot*(1.-1./DOMOversizeFactor_);
                correctedTime += bringForward/photon.GetGroupVelocity();
            }
            
            if (!pmtPhotonSimulator_)
            {
                // add a new hit
                hits->push_back(I3MCHit());
                I3MCHit &hit = hits->back();
                
                // fill in all information
                hit.SetTime(correctedTime);
                hit.SetHitID(photon.GetID());
                hit.SetWeight(1.);
                if (particle) hit.SetParticleID(*particle);
                hit.SetCherenkovDistance(NAN);
                hit.SetHitSource(I3MCHit::SPE); // SPE for now, may be changed by afterpulse simulation
            }
            else
            {
                // make a new hit
                
                // let pmtPhotonSimulator add the hit and all afterpulses
                I3MCHit hit;

                // fill in all information
                hit.SetTime(correctedTime);
                hit.SetHitID(photon.GetID());
                hit.SetWeight(1.);
                if (particle) hit.SetParticleID(*particle);
                hit.SetCherenkovDistance(NAN);

                pmtPhotonSimulator_->ApplyAfterPulseLatePulseAndJitterSim
                (key, hit, *hits);
            }
        }
        
        if (hits) {
        	// sort the photons in each hit series by time
            std::sort(hits->begin(), hits->end(), MCHitTimeLess);
        }
    }
    
    // store the output I3MCHitSeriesMap
    frame->Put(outputMCHitSeriesMapName_, outputMCHitSeriesMap);
    
    // that's it!
    PushFrame(frame);
}
