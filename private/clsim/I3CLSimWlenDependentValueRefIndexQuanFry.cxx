#include <icetray/serialization.h>
#include <icetray/I3Units.h>
#include <clsim/I3CLSimWlenDependentValueRefIndexQuanFry.h>

#include <typeinfo>
#include <cmath>

#include "clsim/to_float_string.h"
using namespace I3CLSimHelper;

const double I3CLSimWlenDependentValueRefIndexQuanFry::default_salinity=38.44*I3Units::perThousand;       // in ppt
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_temperature=13.1;     // in degC
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_pressure=240.0 * 1.01325*I3Units::bar;       // in atm
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_n0  = 1.31405;        // coefficients 0-10
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_n1  = 1.45e-5;
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_n2  = 1.779e-4;
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_n3  = 1.05e-6;
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_n4  = 1.6e-8;
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_n5  = 2.02e-6;
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_n6  = 15.868;
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_n7  = 0.01155;
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_n8  = 0.00423;
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_n9  = 4382.;
const double I3CLSimWlenDependentValueRefIndexQuanFry::default_n10 = 1.1455e6;

I3CLSimWlenDependentValueRefIndexQuanFry::
I3CLSimWlenDependentValueRefIndexQuanFry(double salinity,
                                         double temperature,
                                         double pressure,
                                         double n0,
                                         double n1,
                                         double n2,
                                         double n3,
                                         double n4,
                                         double n5,
                                         double n6,
                                         double n7,
                                         double n8,
                                         double n9,
                                         double n10
                                         )
:
salinity_(salinity/I3Units::perThousand),
temperature_(temperature),
pressure_(pressure/1.01325*I3Units::bar),
n0_(n0),
n1_(n1),
n2_(n2),
n3_(n3),
n4_(n4),
n5_(n5),
n6_(n6),
n7_(n7),
n8_(n8),
n9_(n9),
n10_(n10)
{ 
    UpdateMutables();   
}

void I3CLSimWlenDependentValueRefIndexQuanFry::UpdateMutables() const
{
    a01 = n0_+(n2_-n3_*temperature_+n4_*temperature_*temperature_)*salinity_-n5_*temperature_*temperature_ + n1_*pressure_;
    a2 = n6_+n7_*salinity_-n8_*temperature_;
    a3 = -n9_;
    a4 = n10_;
}

I3CLSimWlenDependentValueRefIndexQuanFry::~I3CLSimWlenDependentValueRefIndexQuanFry() 
{;}


double I3CLSimWlenDependentValueRefIndexQuanFry::GetValue(double wlen) const
{
    const double x = I3Units::nanometer/wlen;
	return a01 + x*(a2 + x*(a3 + x*a4));
}

double I3CLSimWlenDependentValueRefIndexQuanFry::GetDerivative(double wlen) const
{
	const double x = I3Units::nanometer/wlen;
	return -x*x*(a2 + x*(2.0f*a3 + x*3.0f*a4));
}

std::string I3CLSimWlenDependentValueRefIndexQuanFry::GetOpenCLFunction(const std::string &functionName) const
{
    std::string funcDef = 
    std::string("inline float ") + functionName + std::string("(float wavelength)\n");

    std::string funcBody = std::string() + 
    "{\n"
    "    const float a01 = " + to_float_string(a01) + ";\n"
    "    const float a2 = "  + to_float_string(a2) + ";\n"
    "    const float a3 = "  + to_float_string(a3) + ";\n"
    "    const float a4 = "  + to_float_string(a4) + ";\n"
    "    \n"
    "    const float x = " + to_float_string(I3Units::nanometer) + "/wavelength;\n"
    "    return a01 + x*(a2 + x*(a3 + x*a4));\n"
    "}\n"
    ;
    
    return funcDef + funcBody;
}

std::string I3CLSimWlenDependentValueRefIndexQuanFry::GetOpenCLFunctionDerivative(const std::string &functionName) const
{
    std::string funcDef = 
    std::string("inline float ") + functionName + std::string("(float wavelength)\n");
    
    std::string funcBody = std::string() + 
    "{\n"
    "    const float a2 = " + to_float_string(a2) + ";\n"
    "    const float a3 = " + to_float_string(a3) + ";\n"
    "    const float a4 = " + to_float_string(a4) + ";\n"
    "    \n"
    "    const float x = " + to_float_string(I3Units::nanometer) + "/wavelength;\n"
    "    return -x*x*(a2 + x*(2.0f*a3 + x*3.0f*a4));\n"
    "}\n"
    ;
    
    return funcDef + funcBody;
}

bool I3CLSimWlenDependentValueRefIndexQuanFry::CompareTo(const I3CLSimWlenDependentValue &other) const
{
    try
    {
        const I3CLSimWlenDependentValueRefIndexQuanFry &other_ = dynamic_cast<const I3CLSimWlenDependentValueRefIndexQuanFry &>(other);
        return ((other_.salinity_ == salinity_) &&
                (other_.temperature_ == temperature_) &&
                (other_.pressure_ == pressure_) &&
                (other_.n0_ == n0_) &&
                (other_.n1_ == n1_) &&
                (other_.n2_ == n2_) &&
                (other_.n3_ == n3_) &&
                (other_.n4_ == n4_) &&
                (other_.n5_ == n5_) &&
                (other_.n6_ == n6_) &&
                (other_.n7_ == n7_) &&
                (other_.n8_ == n8_) &&
                (other_.n9_ == n9_) &&
                (other_.n10_ == n10_));
    }
    catch (const std::bad_cast& e)
    {
        // not of the same type, treat it as non-equal
        return false;
    }
    
}


template <class Archive>
void I3CLSimWlenDependentValueRefIndexQuanFry::serialize(Archive &ar, unsigned version)
{
    if (version>i3clsimwlendependentvaluerefindexquanfry_version_)
        log_fatal("Attempting to read version %u from file but running version %u of I3CLSimWlenDependentValueRefIndexQuanFry class.",version,i3clsimwlendependentvaluerefindexquanfry_version_);

    ar & make_nvp("I3CLSimWlenDependentValue", base_object<I3CLSimWlenDependentValue>(*this));
    ar & make_nvp("salinity", salinity_);
    ar & make_nvp("temperature", temperature_);
    ar & make_nvp("pressure", pressure_);
    ar & make_nvp("n0", n0_);
    ar & make_nvp("n1", n1_);
    ar & make_nvp("n2", n2_);
    ar & make_nvp("n3", n3_);
    ar & make_nvp("n4", n4_);
    ar & make_nvp("n5", n5_);
    ar & make_nvp("n6", n6_);
    ar & make_nvp("n7", n7_);
    ar & make_nvp("n8", n8_);
    ar & make_nvp("n9", n9_);
    ar & make_nvp("n10", n10_);
    
    if (Archive::is_loading::value)
    {
        UpdateMutables();
    }
}


I3_SERIALIZABLE(I3CLSimWlenDependentValueRefIndexQuanFry);
