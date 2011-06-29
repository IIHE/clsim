#include <icetray/serialization.h>
#include <clsim/I3CLSimParticleParameterization.h>

#include <limits>

I3CLSimParticleParameterization::I3CLSimParticleParameterization()
:
#ifdef I3PARTICLE_SUPPORTS_PDG_ENCODINGS
forPdgEncoding(0),
#else
forParticleType(I3Particle::unknown),
#endif
fromEnergy(0.),
toEnergy(std::numeric_limits<double>::infinity())
{ 
    
}

I3CLSimParticleParameterization::~I3CLSimParticleParameterization() 
{ 

}

I3CLSimParticleParameterization::I3CLSimParticleParameterization
(I3CLSimParticleToStepConverterPtr converter_,
 I3Particle::ParticleType forParticleType_,
 double fromEnergy_, double toEnergy_,
 bool needsLength_
 )
:
converter(converter_),
#ifdef I3PARTICLE_SUPPORTS_PDG_ENCODINGS
forPdgEncoding(I3Particle::ConvertToPdgEncoding(forParticleType_)),
#else
forParticleType(forParticleType_),
#endif
fromEnergy(fromEnergy_),
toEnergy(toEnergy_),
needsLength(needsLength_)
{
    
}

I3CLSimParticleParameterization::I3CLSimParticleParameterization
(I3CLSimParticleToStepConverterPtr converter_,
 const I3Particle &forParticleType_,
 double fromEnergy_, double toEnergy_,
 bool needsLength_
 )
:
converter(converter_),
#ifdef I3PARTICLE_SUPPORTS_PDG_ENCODINGS
forPdgEncoding(forParticleType_.GetPdgEncoding()),
#else
forParticleType(forParticleType_.GetType()),
#endif
fromEnergy(fromEnergy_),
toEnergy(toEnergy_),
needsLength(needsLength_)
{
    
}

bool I3CLSimParticleParameterization::IsValidForParticle(const I3Particle &particle) const
{
    if (isnan(particle.GetEnergy())) {
        log_warn("I3CLSimParticleParameterization::IsValid() called with particle with NaN energy. Parameterization is NOT valid.");
        return false;
    }
    
    return IsValid(particle.GetType(), particle.GetEnergy(), particle.GetLength());
}

bool I3CLSimParticleParameterization::IsValid(I3Particle::ParticleType type, double energy, double length) const
{
    if (isnan(energy)) return false;
#ifdef I3PARTICLE_SUPPORTS_PDG_ENCODINGS
    const int32_t encoding = I3Particle::ConvertToPdgEncoding(type);
    if (encoding==0) return false;
    if (encoding != forPdgEncoding) return false;
#else
    if (type != forParticleType) return false;
#endif
    if ((energy < fromEnergy) || (energy > toEnergy)) return false;
    if ((needsLength) && (isnan(length))) return false;
    
    return true;
}

#ifdef I3PARTICLE_SUPPORTS_PDG_ENCODINGS
bool I3CLSimParticleParameterization::IsValidForPdgEncoding(int32_t encoding, double energy, double length) const
{
    if (isnan(energy)) return false;
    if (encoding != forPdgEncoding) return false;
    if ((energy < fromEnergy) || (energy > toEnergy)) return false;
    if ((needsLength) && (isnan(length))) return false;
    
    return true;
}
#endif
