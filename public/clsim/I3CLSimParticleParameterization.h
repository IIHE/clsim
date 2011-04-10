//
//   Copyright (c) 2011  Claudio Kopper
//   
//   $Id$
//
//   This file is part of the IceTray module g4sim-interface.
//
//   g4sim-intrface is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 3 of the License, or
//   (at your option) any later version.
//
//   IceTray is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef I3CLSIMPARTICLEPARAMETERIZATION_H_INCLUDED
#define I3CLSIMPARTICLEPARAMETERIZATION_H_INCLUDED

#include "icetray/I3TrayHeaders.h"

#include "dataclasses/physics/I3Particle.h"

#include <vector>

// forward declarations
struct I3CLSimParticleToStepConverter;
I3_POINTER_TYPEDEFS(I3CLSimParticleToStepConverter);

/**
 * @brief Defines a particle parameterization for fast simulation,
 * bypassing Geant4
 */
struct I3CLSimParticleParameterization 
{
public:
    I3CLSimParticleParameterization();
    ~I3CLSimParticleParameterization();

    I3CLSimParticleParameterization(I3CLSimParticleToStepConverterPtr converter_,
                                    I3Particle::ParticleType forParticleType_,
                                    double fromEnergy_, double toEnergy_,
                                    bool needsLength_=false);

    
    I3CLSimParticleToStepConverterPtr converter;
    I3Particle::ParticleType forParticleType;
    double fromEnergy, toEnergy;
    bool needsLength;
    
    bool IsValidForParticle(const I3Particle &particle) const;
    bool IsValid(I3Particle::ParticleType type, double energy, double length=NAN) const;
    
private:
};

inline bool operator==(const I3CLSimParticleParameterization &a, const I3CLSimParticleParameterization &b)
{
    if (a.fromEnergy != b.fromEnergy) return false;
    if (a.toEnergy != b.toEnergy) return false;
    if (a.forParticleType != b.forParticleType) return false;
    if (a.converter != b.converter) return false;
    if (a.needsLength != b.needsLength) return false;
    return true;
}

typedef std::vector<I3CLSimParticleParameterization> I3CLSimParticleParameterizationSeries;

I3_POINTER_TYPEDEFS(I3CLSimParticleParameterization);
I3_POINTER_TYPEDEFS(I3CLSimParticleParameterizationSeries);

#endif //I3CLSIMPARTICLEPARAMETERIZATION_H_INCLUDED
