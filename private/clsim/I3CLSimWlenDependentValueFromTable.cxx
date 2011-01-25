#include <icetray/serialization.h>
#include <clsim/I3CLSimWlenDependentValueFromTable.h>

#include <typeinfo>
#include <cmath>
#include <math.h>
#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include "clsim/to_float_string.h"
using namespace I3CLSimHelper;

I3CLSimWlenDependentValueFromTable::
I3CLSimWlenDependentValueFromTable(double startWlen,
                                   double wlenStep,
                                   const std::vector<double> &values)
:
startWlen_(startWlen),
wlenStep_(wlenStep),
values_(values)
{
    if (startWlen_ < 0.) throw std::range_error("startWlen must not be < 0!");
    if (wlenStep_ <= 0.) throw std::range_error("wlenStep must not be <= 0!");
    if (values_.size() < 2) throw std::length_error("values must contain at least 2 elements!");
}

I3CLSimWlenDependentValueFromTable::I3CLSimWlenDependentValueFromTable() {;}

I3CLSimWlenDependentValueFromTable::~I3CLSimWlenDependentValueFromTable() 
{;}

namespace {
    inline static double modf(double value, double &iptr)
    {
        iptr = trunc(value);
        return copysign(std::isinf(value) ? 0.0 : value - iptr, value);
    }
    
    inline static double mix(double min, double max, double t)
    {
        return min+(max-min)*t;
    }
}

double I3CLSimWlenDependentValueFromTable::GetValue(double wlen) const
{
    double fbin;
    double fraction = modf((wlen-startWlen_)/wlenStep_, &fbin);
    int ibin=static_cast<int>(fbin);

    if (ibin<0) {
        ibin=0;
        fraction=0.;
    } else if (static_cast<std::size_t>(ibin)>=values_.size()-1) {
        ibin=values_.size()-2;
        fraction=1.;
    }
    const unsigned int bin = ibin;

    return mix(values_[bin], 
			   values_[bin+1],
			   fraction);
}

double I3CLSimWlenDependentValueFromTable::GetMinWlen() const
{
    return startWlen_;
}

double I3CLSimWlenDependentValueFromTable::GetMaxWlen() const
{
    return startWlen_ + wlenStep_ * static_cast<double>(values_.size()-1);
}

std::string I3CLSimWlenDependentValueFromTable::GetOpenCLFunction(const std::string &functionName) const
{
    // some names
    const std::string dataName = functionName + "_data";
    const std::string interpHelperName = functionName + "_getInterpolationBinAndFraction";
    
    // define the actual data
    std::string dataDef = std::string() +
    "__constant float " + dataName + "[" + boost::lexical_cast<std::string>(values_.size()) + "] = {\n";
    BOOST_FOREACH(const double &val, values_)
    {
        dataDef += to_float_string(val) + ", ";
    }
    dataDef += "\n";
    dataDef += "};\n\n";

    std::string interpHelperDef =
    std::string("inline void ") + interpHelperName + "(float wavelength, int *bin, float *fraction)\n"
    "{\n"
    "    float fbin;\n"
    "    *fraction = modf((wavelength-" + to_float_string(startWlen_) + ")/" + to_float_string(wlenStep_) + ", &fbin);\n"
    "    \n"
    "    int ibin=(int)fbin;\n"
    "    \n"
    "    if (ibin<0) {\n"
    "        ibin=0;\n"
    "        *fraction=0.f;\n"
    "    } else if (ibin>=" + boost::lexical_cast<std::string>(values_.size()) + "-1) {\n"
    "        ibin=" + boost::lexical_cast<std::string>(values_.size()) + "-2;\n"
    "        *fraction=1.f;\n"
    "    }\n"
    "    \n"
    "    *bin = ibin;\n"
    "}\n\n"
    ;
    
    std::string funcDef = 
    std::string("inline float ") + functionName + std::string("(float wavelength)\n");
    
    std::string funcBody = std::string() + 
    "{\n"
    "    int bin; float fraction;\n"
    "    " + interpHelperName + "(wavelength, &bin, &fraction);\n"
    "    \n"
    "    return mix(" + dataName + "[bin],\n"
    "               " + dataName + "[bin+1],\n"
    "               fraction);\n"
    "}\n"
    ;
    
    return dataDef + interpHelperDef + funcDef + funcBody;
}

bool I3CLSimWlenDependentValueFromTable::CompareTo(const I3CLSimWlenDependentValue &other) const
{
    try
    {
        const I3CLSimWlenDependentValueFromTable &other_ = dynamic_cast<const I3CLSimWlenDependentValueFromTable &>(other);
        if ((other_.startWlen_ != startWlen_) ||
            (other_.wlenStep_ != wlenStep_) ||
            (other_.values_.size() != values_.size()))
            return false;
        
        for (std::size_t i=0;i<values_.size();++i)
        {
            if (other_.values_[i] != values_[i]) return false;
        }
        
        return true;
    }
    catch (const std::bad_cast& e)
    {
        // not of the same type, treat it as non-equal
        return false;
    }
    
}




template <class Archive>
void I3CLSimWlenDependentValueFromTable::serialize(Archive &ar, unsigned version)
{
    if (version>i3clsimwlendependentvaluefromtable_version_)
        log_fatal("Attempting to read version %u from file but running version %u of I3CLSimWlenDependentValueFromTable class.",version,i3clsimwlendependentvaluefromtable_version_);

    ar & make_nvp("I3CLSimWlenDependentValue", base_object<I3CLSimWlenDependentValue>(*this));
    ar & make_nvp("startWlen", startWlen_);
    ar & make_nvp("wlenStep", wlenStep_);
    ar & make_nvp("values", values_);
}     


I3_SERIALIZABLE(I3CLSimWlenDependentValueFromTable);