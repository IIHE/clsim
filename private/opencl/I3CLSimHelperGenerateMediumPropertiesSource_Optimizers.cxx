#include "opencl/I3CLSimHelperGenerateMediumPropertiesSource_Optimizers.h"

#include <sstream>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include <boost/preprocessor.hpp>

#include "icetray/I3Units.h"

#include "clsim/I3CLSimWlenDependentValueAbsLenIceCube.h"
#include "clsim/I3CLSimWlenDependentValueScatLenIceCube.h"

#include "clsim/to_float_string.h"


namespace I3CLSimHelper
{
    
    
    
    // takes a vector of shared_ptrs-to-const and dynamic_casts all
    // contents to a new type. only returns a filled vector if the
    // cast succeeds for all elements. "worked" is set to true
    // if the cast worked and "false" if it did not.
    template<class T, class U>
    std::vector<shared_ptr<const T> > dynamic_vector_contents_cast(const std::vector<shared_ptr<const U> > &inVec, bool &worked)
    {
        std::vector<shared_ptr<const T> > retVec;
        
        BOOST_FOREACH(const shared_ptr<const U> &inputValue, inVec)
        {
            if (!inputValue) {
                retVec.push_back(shared_ptr<const T>());
                continue;
            }
            
            shared_ptr<const T> convertedPtr = boost::dynamic_pointer_cast<const T>(inputValue);
            if (!convertedPtr) {worked=false; return std::vector<shared_ptr<const T> >();}
            
            retVec.push_back(convertedPtr);
        }
        
        worked=true;
        return retVec;
    }
    
    template<typename T, class V>
    bool is_constant(const std::vector<T> &vect, const V &visitor)
    {
        if (vect.size() <= 1) return true;
        
        for (std::size_t i = 1; i<vect.size(); ++i)
        {
            if (!(visitor(vect[0]) == visitor(vect[i]))) return false;
        }
        
        return true;
    }
    
    
    namespace {
        #define CHECK_THESE_FOR_CONSTNESS_ABSLEN                        \
        (Kappa)(A)(B)(D)(E)

        #define CHECK_THESE_FOR_CONSTNESS_SCATLEN                       \
        (Alpha)

        #define GEN_VISITOR(r, data, a)                                 \
        struct BOOST_PP_CAT(visit_,a)                                   \
        {                                                               \
            template<typename T>                                        \
            double operator()(const shared_ptr<T> &ptr) const           \
            {                                                           \
                if (!ptr) return NAN;                                   \
                return BOOST_PP_CAT(ptr->Get,a)();                      \
            }                                                           \
                                                                        \
            template<typename T>                                        \
            double operator()(const shared_ptr<const T> &ptr) const     \
            {                                                           \
                if (!ptr) return NAN;                                   \
                return BOOST_PP_CAT(ptr->Get,a)();                      \
            }                                                           \
        };

        BOOST_PP_SEQ_FOR_EACH(GEN_VISITOR, ~, CHECK_THESE_FOR_CONSTNESS_ABSLEN)
        BOOST_PP_SEQ_FOR_EACH(GEN_VISITOR, ~, CHECK_THESE_FOR_CONSTNESS_SCATLEN)
    }
    
    
    // optimizers (special converters for functions
    // where we can generate more optimized code
    // for layered values)
    std::string GenerateOptimizedCodeFor_I3CLSimWlenDependentValueAbsLenIceCube(const std::vector<I3CLSimWlenDependentValueConstPtr> &layeredFunction,
                                                                                const std::string &fullName,
                                                                                const std::string &functionName,
                                                                                bool &worked)
    {
        // are all of them of type I3CLSimWlenDependentValueAbsLenIceCube?
        const std::vector<I3CLSimWlenDependentValueAbsLenIceCubeConstPtr> layeredFunctionIceCube =
        dynamic_vector_contents_cast<I3CLSimWlenDependentValueAbsLenIceCube>(layeredFunction, worked);
        if (layeredFunctionIceCube.size() <= 1) worked=false;
        if (!worked) return std::string("");

        {
            // this currently only works if kappa, A, B, D and E are constant
            bool isConst=true;
            #define CONST_CHECK(r, data, a) isConst = isConst && is_constant(layeredFunctionIceCube, BOOST_PP_CAT(visit_,a)());
            BOOST_PP_SEQ_FOR_EACH(CONST_CHECK, ~, CHECK_THESE_FOR_CONSTNESS_ABSLEN)
            #undef CONST_CHECK
            if (!isConst) {worked=false; return "";}
        }        
        
        std::ostringstream code;

        code << "///////////////// START " << fullName << " (optimized) ////////////////\n";
        code << "\n";

        code << "__constant float " << functionName << "_aDust400[" << layeredFunctionIceCube.size() << "] = {\n";
        BOOST_FOREACH(const I3CLSimWlenDependentValueAbsLenIceCubeConstPtr &function, layeredFunctionIceCube)
        {
            code << "    " << to_float_string(function->GetADust400()) << ",\n";
        }
        code << "};\n";
        code << "\n";

        code << "__constant float " << functionName << "_deltaTau[" << layeredFunctionIceCube.size() << "] = {\n";
        BOOST_FOREACH(const I3CLSimWlenDependentValueAbsLenIceCubeConstPtr &function, layeredFunctionIceCube)
        {
            code << "    " << to_float_string(function->GetDeltaTau()) << ",\n";
        }
        code << "};\n";
        code << "\n";

        code << "inline float " << functionName << "(unsigned int layer, float wlen)\n";
        code << "{\n";
        
        code << "    const float kappa = " << to_float_string(layeredFunctionIceCube[0]->GetKappa()) << ";\n";
        code << "    const float A = " << to_float_string(layeredFunctionIceCube[0]->GetA()) << ";\n";
        code << "    const float B = " << to_float_string(layeredFunctionIceCube[0]->GetB()) << ";\n";
        code << "    const float D = " << to_float_string(layeredFunctionIceCube[0]->GetD()) << ";\n";
        code << "    const float E = " << to_float_string(layeredFunctionIceCube[0]->GetE()) << ";\n";
        code << "    \n";
        code << "    const float x = wlen/" << to_float_string(I3Units::nanometer) << ";\n";
        code << "    \n";
        code << "#ifdef USE_NATIVE_MATH\n";
        code << "    return " << to_float_string(I3Units::meter) << "*native_recip( (D*" << functionName << "_aDust400[layer]+E) * native_powr(x, -kappa)  +  A*native_exp(-B/x) * (1.f + 0.01f*" << functionName << "_deltaTau[layer]) );\n";
        code << "#else\n";
        code << "    return " << to_float_string(I3Units::meter) << "/( (D*" << functionName << "_aDust400[layer]+E) * powr(x, -kappa)  +  A*exp(-B/x) * (1.f + 0.01f*" << functionName << "_deltaTau[layer]) );\n";
        code << "#endif\n";
        
        code << "}\n";
        code << "\n";
        
        code << "///////////////// END " << fullName << " (optimized) ////////////////\n";
        code << "\n";        

        worked=true;
        return code.str();
    }
    
    
    
    
    std::string GenerateOptimizedCodeFor_I3CLSimWlenDependentValueScatLenIceCube(const std::vector<I3CLSimWlenDependentValueConstPtr> &layeredFunction,
                                                                                 const std::string &fullName,
                                                                                 const std::string &functionName,
                                                                                 bool &worked)
    {
        // are all of them of type I3CLSimWlenDependentValueScatLenIceCube?
        const std::vector<I3CLSimWlenDependentValueScatLenIceCubeConstPtr> layeredFunctionIceCube =
        dynamic_vector_contents_cast<I3CLSimWlenDependentValueScatLenIceCube>(layeredFunction, worked);
        if (layeredFunctionIceCube.size() <= 1) worked=false;
        if (!worked) return std::string("");
        
        {
            // this currently only works if alpha is constant
            bool isConst=true;
#define CONST_CHECK(r, data, a) isConst = isConst && is_constant(layeredFunctionIceCube, BOOST_PP_CAT(visit_,a)());
            BOOST_PP_SEQ_FOR_EACH(CONST_CHECK, ~, CHECK_THESE_FOR_CONSTNESS_SCATLEN)
#undef CONST_CHECK
            if (!isConst) {worked=false; return "";}
        }        
        
        std::ostringstream code;
        
        code << "///////////////// START " << fullName << " (optimized) ////////////////\n";
        code << "\n";
        
        code << "__constant float " << functionName << "_be400[" << layeredFunctionIceCube.size() << "] = {\n";
        BOOST_FOREACH(const I3CLSimWlenDependentValueScatLenIceCubeConstPtr &function, layeredFunctionIceCube)
        {
            code << "    " << to_float_string(function->GetBe400()) << ",\n";
        }
        code << "};\n";
        code << "\n";
        
        code << "inline float " << functionName << "(unsigned int layer, float wlen)\n";
        code << "{\n";
        
        const std::string refWlenAsString = to_float_string(1./(400.*I3Units::nanometer));

        code << "    const float alpha = " << to_float_string(layeredFunctionIceCube[0]->GetAlpha()) << ";\n";
        code << "    \n";
        code << "    const float x = wlen/" << to_float_string(I3Units::nanometer) << ";\n";
        code << "    \n";
        code << "    \n";
        code << "#ifdef USE_NATIVE_MATH\n";
        code << "    return " << to_float_string(I3Units::meter) << "*native_recip( " << functionName << "_be400[layer] * native_powr(wlen*" + refWlenAsString + ", -alpha) );\n";
        code << "#else\n";
        code << "    return " << to_float_string(I3Units::meter) << "/( " << functionName << "_be400[layer] * powr(wlen*" + refWlenAsString + ", -alpha) );\n";
        code << "#endif\n";
        
        code << "}\n";
        code << "\n";
        
        code << "///////////////// END " << fullName << " (optimized) ////////////////\n";
        code << "\n";        

        worked=true;
        return code.str();
    }
    
};

