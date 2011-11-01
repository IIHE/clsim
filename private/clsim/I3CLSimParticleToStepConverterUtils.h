#ifndef I3CLSIMPARTICLETOSTEPCONVERTERUTILS_H_INCLUDED
#define I3CLSIMPARTICLETOSTEPCONVERTERUTILS_H_INCLUDED

#include "dataclasses/physics/I3Particle.h"
#include "phys-services/I3RandomService.h"
#include "dataclasses/I3Constants.h"

#include "clsim/I3CLSimStep.h"
#include "clsim/I3CLSimWlenDependentValue.h"

#include <cmath>

namespace I3CLSimParticleToStepConverterUtils 
{
    
    double NumberOfPhotonsPerMeter(const I3CLSimWlenDependentValue &phaseRefIndex,
                                   const I3CLSimWlenDependentValue &wavelengthGenerationBias,
                                   double fromWlen, double toWlen);
    
    // stolen from PPC by D. Chirkin
    inline double gammaDistributedNumber(double shape, I3RandomService &randomService)
    {
        double x;
        if(shape<1.){  // Weibull algorithm
            double c=1./shape;
            double d=(1.-shape)*std::pow(shape, shape / (1.-shape) );
            double z, e;
            do
            {
                z=-std::log(randomService.Uniform());
                e=-std::log(randomService.Uniform());
                x=std::pow(z, c);
            } while(z+e<d+x); // or here
        }
        else  // Cheng's algorithm
        {
            double b=shape-std::log(4.0);
            double l=std::sqrt(2.*shape-1.0);
            const double cheng=1.0+std::log(4.5);
            
            //float u, v;
            float y, z, r;
            do
            {
                const double rx = randomService.Uniform();
                const double ry = randomService.Uniform();
                
                y=log( ry/(1.-ry) ) / l;
                x=shape*std::exp(y);
                z=rx*ry*ry;
                r=b+(shape+l)*y-x;
            } while(r<4.5*z-cheng && r<std::log(z));
        }
        
        return x;
    }
    
    // smart pointer version
    inline double gammaDistributedNumber(double shape, I3RandomServicePtr randomService)
    {
        return gammaDistributedNumber(shape, *randomService);
    }
    
    // in-place rotation
    inline void scatterDirectionByAngle(double cosa, double sina,
                                        double &x, double &y, double &z,
                                        I3RandomService &randomService)
    {
        // randomize direction of scattering (rotation around old direction axis)
        const double b=2.0*M_PI*randomService.Uniform();
        const double cosb=std::cos(b);
        const double sinb=std::sin(b);
        
        // Rotate new direction into absolute frame of reference 
        const double sinth = std::sqrt(std::max(0., 1.-z*z));
        
        if(sinth>0.){  // Current direction not vertical, so rotate 
            const double old_x=x;
            const double old_y=y;
            const double old_z=z;
            
            x=old_x*cosa-(old_y*cosb+old_z*old_x*sinb)*sina/sinth;
            y=old_y*cosa+(old_x*cosb-old_z*old_y*sinb)*sina/sinth;
            z=old_z*cosa+sina*sinb*sinth;
        }else{         // Current direction is vertical, so this is trivial
            x=sina*cosb;
            y=sina*sinb;
            if (z>=0.) {
                z=cosa;
            } else {
                z=-cosa;
            }
        }
        
        {
            const double recip_length = 1./std::sqrt( x*x + y*y + z*z );
            
            x *= recip_length;
            y *= recip_length;
            z *= recip_length;
        }
    }
    
    inline void scatterDirectionByAngle(double cosa, double sina,
                                        double &x, double &y, double &z,
                                        I3RandomServicePtr randomService)
    {
        scatterDirectionByAngle(cosa, sina, x, y, z, *randomService);
    }
    
    
    class GenerateStepPreCalculator
    {
    public:
        GenerateStepPreCalculator(I3RandomServicePtr randomService,
                                  float angularDist_a=0.39,
                                  float angularDist_b=2.61,
                                  std::size_t numberOfValues=102400);
        ~GenerateStepPreCalculator();
        
        inline void GetAngularCosSinValue(float &angular_cos, float &angular_sin)
        {
            if (index_ >= numberOfValues_) RegenerateValues();
            
            angular_cos = angular_cos_cache_[index_];
            angular_sin = angular_sin_cache_[index_];
            
            ++index_;
        }
        
    private:
        I3RandomServicePtr randomService_;
        
        float angularDist_a_;
        std::vector<float> one_over_angularDist_a_;
        float angularDist_b_;
        float angularDist_I_;
        
        std::vector<float> angular_sin_cache_;
        std::vector<float> angular_cos_cache_;
        
        std::vector<float> randomNumber_workspace_;
        
        std::vector<float> scratch_space1_;
        std::vector<float> scratch_space2_;

        std::size_t numberOfValues_;
        std::size_t index_;
        
        void RegenerateValues();
    };
    
    
    inline void GenerateStep(I3CLSimStep &newStep,
                             const I3Particle &p,
                             uint32_t identifier,
                             I3RandomService &randomService,
                             uint32_t photonsPerStep,
                             const double &longitudinalPos,
                             GenerateStepPreCalculator &preCalc)
    {
        /*
        const double angularDist_a=0.39;
        const double angularDist_b=2.61;
        const double angularDist_I=1.-std::exp(-angularDist_b*std::pow(2., angularDist_a));
        
        const double rndVal = randomService.Uniform();
        const double angular_cos=std::max(1.-std::pow(-std::log(1.-rndVal*angularDist_I)/angularDist_b, 1./angularDist_a), -1.0);
        const double angular_sin=std::sqrt(1.-angular_cos*angular_cos);
        */
        
        float angular_cos, angular_sin;
        preCalc.GetAngularCosSinValue(angular_cos, angular_sin);
        
        double step_dx = p.GetDir().GetX();
        double step_dy = p.GetDir().GetY();
        double step_dz = p.GetDir().GetZ();
        
        // set all values
        newStep.SetPosX(p.GetX() + longitudinalPos*step_dx);
        newStep.SetPosY(p.GetY() + longitudinalPos*step_dy);
        newStep.SetPosZ(p.GetZ() + longitudinalPos*step_dz);
        newStep.SetTime(p.GetTime() + longitudinalPos/I3Constants::c);
        
        newStep.SetLength(1.*I3Units::mm);
        newStep.SetNumPhotons(photonsPerStep);
        newStep.SetWeight(1.);
        newStep.SetBeta(1.);
        newStep.SetID(identifier);

        // rotate in-place
        scatterDirectionByAngle(angular_cos, angular_sin,
                                step_dx, step_dy, step_dz,
                                randomService);
        
        newStep.SetDir(step_dx, step_dy, step_dz);
        
        
    }
    
    inline void GenerateStep(I3CLSimStep &newStep,
                             const I3Particle &p,
                             uint32_t identifier,
                             I3RandomServicePtr randomService,
                             uint32_t photonsPerStep,
                             const double &longitudinalPos,
                             GenerateStepPreCalculator &preCalc)
    {
        GenerateStep(newStep, p, identifier, *randomService, photonsPerStep, longitudinalPos, preCalc);
    }

    
    inline void GenerateStepForMuon(I3CLSimStep &newStep,
                                    const I3Particle &p,
                                    uint32_t identifier,
                                    uint32_t photonsPerStep,
                                    double length)
    {
        // set all values
        newStep.SetPosX(p.GetX());
        newStep.SetPosY(p.GetY());
        newStep.SetPosZ(p.GetZ());
        newStep.SetDir(p.GetDir().GetX(), p.GetDir().GetY(), p.GetDir().GetZ());
        newStep.SetTime(p.GetTime());
        
        newStep.SetLength(length);
        newStep.SetNumPhotons(photonsPerStep);
        newStep.SetWeight(1.);
        newStep.SetBeta(1.);
        newStep.SetID(identifier);
    }
    

}


#endif //I3CLSIMPARTICLETOSTEPCONVERTERUTILS_H_INCLUDED
