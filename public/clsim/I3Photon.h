/**
 * copyright (C) 2011
 * Claudio Kopper <claudio.kopper@nikhef.nl>
 * $Id$
 *
 * @file I3Photon.h
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

#ifndef I3PHOTON_H_INCLUDED
#define I3PHOTON_H_INCLUDED

#include <stdexcept>

#include "icetray/I3FrameObject.h"

#include "dataclasses/I3Vector.h"
#include "dataclasses/I3Map.h"
#include "icetray/OMKey.h"
#include "dataclasses/I3Direction.h"
#include "dataclasses/I3Position.h"
#include "dataclasses/physics/I3Particle.h"

/**
 * @brief This class contains a photon with
 * simulated arrival time, the direction
 * the photon arrived from, the position
 * on the OM the photon hit (stored as an 
 * direction from the OM center to the hit
 * position) and the photon's wavelength.
 */
static const unsigned i3photon_version_ = 1;

class I3Photon : public I3FrameObject
{
    
public:
    
    I3Photon() : 
    time_(NAN), 
    weight_(NAN), 
    ID_(-1),
    particleID_(-1), 
    particleMajorID_(0), 
    cherenkovDist_(NAN),
    wavelength_(NAN),
    groupVelocity_(NAN),
    direction_(),
    position_(),
    startTime_(NAN),
    startDirection_(),
    startPosition_(),
    numScattered_(0)
    { }
    
    I3Photon(uint64_t mid, int id) : 
    time_(NAN), 
    weight_(NAN), 
    ID_(-1),
    particleID_(id), 
    particleMajorID_(mid), 
    cherenkovDist_(NAN),
    wavelength_(NAN),
    groupVelocity_(NAN),
    direction_(),
    position_(),
    startTime_(NAN),
    startDirection_(),
    startPosition_(),
    numScattered_(0)
    { }
    
    virtual ~I3Photon();
    
    double GetTime() const {return time_;}
    
    void SetTime(double time) {time_ = time;}
    
    int32_t GetID() const {return ID_;}
    
    void SetID(int32_t ID) {ID_ = ID;}
    
    double GetWeight() const {return weight_; }
    
    void SetWeight(double weight) {weight_ = weight; }
    
    int32_t GetParticleMinorID() const { return particleID_; }
    
    uint64_t GetParticleMajorID() const { return particleMajorID_; }

    void SetParticleMinorID(int32_t minorID) { particleID_ = minorID; }
    
    void SetParticleMajorID(uint64_t majorID) { particleMajorID_ = majorID; }

    void SetParticleID(const I3Particle&);
    
    /**
     * @return the path distance to the track which emitted this photon.
     * This is the full path with all possible scatters.
     */
    double GetCherenkovDist() const {return cherenkovDist_; }
    
    /**
     * @param CherenkovDist set the path distance to track which 
     * emitted this photon.
     * This is the full path with all possible scatters.
     */
    void SetCherenkovDist(double CherenkovDist) {cherenkovDist_ = CherenkovDist; }
    
    /**
     * @return The full time it took the photon to travel from its emission
     * point to the OM.
     */
    double GetCherenkovTime() const {return time_-startTime_; }
    
    double GetWavelength() const {return wavelength_;}
    
    void SetWavelength(double wlen) {wavelength_ = wlen;}

    double GetGroupVelocity() const {return groupVelocity_;}
    
    void SetGroupVelocity(double gvel) {groupVelocity_ = gvel;}

    /**
     * @return The position of this photon on the OM. This
     * position is relative to the global coordinate system, where the
     * z-axis is facing upwards. It is NOT relative to the OM axis!!
     */
    const I3Position& GetPos() const { return position_; }
    
    /**
     * @param p The position of this on the OM. This
     * position is relative to the global coordinate system, where the
     * z-axis is facing upwards. It is NOT relative to the OM axis!!
     */
    void SetPos(const I3Position& p) { position_.SetPosition(p); }
    
    /**
     * @return The direction vector of the photon on the OM. 
     * This direction is relative to the global coordinate system, where the
     * z-axis is facing upwards. It is NOT relative to the OM axis!!
     */
    const I3Direction& GetDir() const { return direction_; }
    
    /**
     * @param d The direction vector of the photon on the OM. 
     * This direction is relative to the global coordinate system, where the
     * z-axis is facing upwards. It is NOT relative to the OM axis!!
     */
    void SetDir(const I3Direction& d) { direction_.SetDirection(d); }

    
    /**
     * @return The time of emission of this photon.
     */
    double GetStartTime() const {return startTime_;}
    
    /**
     * @param p The time of emission of this photon.
     */
    void SetStartTime(double time) {startTime_ = time;}
    
    /**
     * @return The position of emission of this photon.
     */
    const I3Position& GetStartPos() const { return startPosition_; }
    
    /**
     * @param p The position of emission of this photon.
     */
    void SetStartPos(const I3Position& p) { startPosition_.SetPosition(p); }
    
    /**
     * @return The direction of emission of this photon.
     */
    const I3Direction& GetStartDir() const { return startDirection_; }
    
    /**
     * @param d The direction of emission of this photon.
     */
    void SetStartDir(const I3Direction& d) { startDirection_.SetDirection(d); }

    
    /** 
     * @return this returns the number of times this photon was scattered.
     */
    inline uint32_t GetNumScattered() const {return numScattered_;}
    
    /** 
     * this sets the number of times this photon was scattered.
     */
    inline void SetNumScattered(uint32_t numScattered) {
        if (static_cast<std::size_t>(numScattered) < intermediatePositions_.size())
            throw std::runtime_error("numScattered cannot be smaller than the number of intermediate positions");

        numScattered_=numScattered;
    }
    
    /** 
     * @return this returns the number of positions where this photon has been recorded.
     * This will be the number of scatter events returned by GetNumScattered()+2
     * [i.e. including the inital and final position].
     */
    inline uint32_t GetNumPositionListEntries() const {
        if (intermediatePositions_.size() > static_cast<std::size_t>(numScattered_))
            throw std::logic_error("I3Photon has inconsistent internal state.");
        return numScattered_+2;
    }

    /** 
     * @return this returns the position of the photon at a certain index,
     * with index < GetNumPositionListEntries().
     *
     * The return value may be an invalid shared_ptr if there is no position
     * stored for a certain index.
     */
    inline I3PositionConstPtr GetPositionListEntry(uint32_t index) const
    {
        if (intermediatePositions_.size() > static_cast<std::size_t>(numScattered_))
            throw std::logic_error("I3Photon has inconsistent internal state.");
        if (index >= numScattered_+2)
            throw std::runtime_error("invalid index");

        if (index==0) {
            return I3PositionConstPtr(new I3Position(startPosition_));
        } else if (index==numScattered_+1) {
            return I3PositionConstPtr(new I3Position(position_));
        } else {
            const std::size_t num_empty_entries = static_cast<std::size_t>(numScattered_)-intermediatePositions_.size();
            if (index-1 < num_empty_entries) {
                return I3PositionConstPtr(); // this entry has not been saved
            } else {
                return I3PositionConstPtr(new I3Position(intermediatePositions_[index-1-num_empty_entries]));
            }
        }
    }
    
    /**
     * Appends a position to the back of the intermediate position list.
     * The final list will be: [startPos] + intermediatePos + [finalPos].
     *
     * Use SetNumScattered() first to set the total number of scatters
     * and then add at most that number of positions.
     */
    inline void AppendToIntermediatePositionList(const I3Position &pos)
    {
        if (intermediatePositions_.size() >= static_cast<std::size_t>(numScattered_))
            throw std::runtime_error(std::string("Use SetNumScattered() first to increase the total number of scatters before adding more scatter positions. numScattered=") + boost::lexical_cast<std::string>(numScattered_) + ", intermediatePositions.size()=" + boost::lexical_cast<std::string>(intermediatePositions_.size()));

        intermediatePositions_.push_back(pos);
    }

    bool operator==(const I3Photon& rhs) {
        if (!( time_ == rhs.time_
        && startTime_ == rhs.startTime_
        && ID_ == rhs.ID_
        && weight_ == rhs.weight_
        && particleID_ == rhs.particleID_
        && particleMajorID_ == rhs.particleMajorID_
        && cherenkovDist_ == rhs.cherenkovDist_
        && wavelength_ == rhs.wavelength_
        && direction_.GetAzimuth() == rhs.direction_.GetAzimuth()
        && direction_.GetZenith() == rhs.direction_.GetZenith()
        && position_.GetX() == rhs.position_.GetX()
        && position_.GetY() == rhs.position_.GetY()
        && position_.GetZ() == rhs.position_.GetZ()
        && startDirection_.GetAzimuth() == rhs.startDirection_.GetAzimuth()
        && startDirection_.GetZenith() == rhs.startDirection_.GetZenith()
        && startPosition_.GetX() == rhs.startPosition_.GetX()
        && startPosition_.GetY() == rhs.startPosition_.GetY()
        && startPosition_.GetZ() == rhs.startPosition_.GetZ()
        && groupVelocity_ == rhs.groupVelocity_
        && numScattered_ == rhs.numScattered_))
            return false;
        
        if (intermediatePositions_.size() != rhs.intermediatePositions_.size()) return false;
        
        for (std::size_t i=0;i<intermediatePositions_.size();++i)
        {
            if (intermediatePositions_[i].GetX() != rhs.intermediatePositions_[i].GetX()) return false;
            if (intermediatePositions_[i].GetY() != rhs.intermediatePositions_[i].GetY()) return false;
            if (intermediatePositions_[i].GetZ() != rhs.intermediatePositions_[i].GetZ()) return false;
        }
        
        return true;
    }
    
private:
    double time_;
    double weight_;
    int32_t ID_;
    int32_t particleID_;
    uint64_t particleMajorID_;
    double cherenkovDist_;
    double wavelength_;
    double groupVelocity_;
    
    I3Direction direction_;
    I3Position position_;

    double startTime_;
    I3Direction startDirection_;
    I3Position startPosition_;

    uint32_t numScattered_;

    std::vector<I3Position> intermediatePositions_;
    
    friend class boost::serialization::access;
    
    template <class Archive> void serialize(Archive & ar, unsigned version);
    
};

BOOST_CLASS_VERSION(I3Photon, i3photon_version_);

typedef I3Vector<I3Photon> I3PhotonSeries;
typedef I3Map<OMKey, I3PhotonSeries> I3PhotonSeriesMap; 

I3_POINTER_TYPEDEFS(I3Photon);
I3_POINTER_TYPEDEFS(I3PhotonSeries);
I3_POINTER_TYPEDEFS(I3PhotonSeriesMap);

#endif //I3PHOTON_H_INCLUDED
