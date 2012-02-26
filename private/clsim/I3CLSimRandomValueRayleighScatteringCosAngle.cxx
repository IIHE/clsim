#include <icetray/serialization.h>
#include <icetray/I3Logging.h>
#include <clsim/I3CLSimRandomValueRayleighScatteringCosAngle.h>

#include "clsim/to_float_string.h"
using namespace I3CLSimHelper;

I3CLSimRandomValueRayleighScatteringCosAngle::I3CLSimRandomValueRayleighScatteringCosAngle()
{ 
}

I3CLSimRandomValueRayleighScatteringCosAngle::~I3CLSimRandomValueRayleighScatteringCosAngle() 
{ 

}

std::string I3CLSimRandomValueRayleighScatteringCosAngle::GetOpenCLFunction
(const std::string &functionName,
 const std::string &functionArgs,
 const std::string &functionArgsToCall,
 const std::string &uniformRandomCall_co,
 const std::string &uniformRandomCall_oc
 ) const
{
    const std::string functionDecl = std::string("inline float ") + functionName + "(" + functionArgs + ")";
    
    return functionDecl + ";\n\n" + functionDecl + "\n"
    "{\n"
    "    const float b = 0.835f;\n"
    "    const float p = 1.f/0.835f;\n"
    "    \n"
    "    const float q = (b+3.f)*((" + uniformRandomCall_co + ")-0.5f)/b;\n"
    "    const float d = q*q + p*p*p;\n"
    "    \n"
    "#ifdef USE_NATIVE_MATH\n"
    "    const float u1 = -q+native_sqrt(d);\n"
    "    const float u = native_powr((fabs(u1)),(1.f/3.f)) * sign(u1);\n"
    "    \n"
    "    const float v1 = -q-native_sqrt(d);\n"
    "    const float v = native_powr((fabs(v1)),(1.f/3.f)) * sign(v1);\n"
    "#else\n"
    "    const float u1 = -q+sqrt(d);\n"
    "    const float u = powr((fabs(u1)),(1.f/3.f)) * sign(u1);\n"
    "    \n"
    "    const float v1 = -q-sqrt(d);\n"
    "    const float v = powr((fabs(v1)),(1.f/3.f)) * sign(v1);\n"
    "#endif\n"
    "    \n"
    "    return clamp(u+v, -1.f, 1.f);\n"
    "}\n"
    ;
}

bool I3CLSimRandomValueRayleighScatteringCosAngle::CompareTo(const I3CLSimRandomValue &other) const
{
    try
    {
        //const I3CLSimRandomValueRayleighScatteringCosAngle &other_ = dynamic_cast<const I3CLSimRandomValueRayleighScatteringCosAngle &>(other);
        return true; // does not have internal state
    }
    catch (const std::bad_cast& e)
    {
        // not of the same type, treat it as non-equal
        return false;
    }
    
}

template <class Archive>
void I3CLSimRandomValueRayleighScatteringCosAngle::serialize(Archive &ar, unsigned version)
{
    if (version>i3clsimrandomvaluerayleighscatteringcosangle_version_)
        log_fatal("Attempting to read version %u from file but running version %u of I3CLSimRandomValueRayleighScatteringCosAngle class.",version,i3clsimrandomvaluerayleighscatteringcosangle_version_);

    ar & make_nvp("I3CLSimRandomValue", base_object<I3CLSimRandomValue>(*this));
}     


I3_SERIALIZABLE(I3CLSimRandomValueRayleighScatteringCosAngle);
