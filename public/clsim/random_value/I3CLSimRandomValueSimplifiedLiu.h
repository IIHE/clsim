/**
 * Copyright (c) 2011, 2012
 * Claudio Kopper <claudio.kopper@icecube.wisc.edu>
 * and the IceCube Collaboration <http://www.icecube.wisc.edu>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * $Id$
 *
 * @file I3CLSimRandomValueSimplifiedLiu.h
 * @version $Revision$
 * @date $Date$
 * @author Claudio Kopper
 */

#ifndef I3CLSIMRANDOMVALUESIMPLIFIEDLIU_H_INCLUDED
#define I3CLSIMRANDOMVALUESIMPLIFIEDLIU_H_INCLUDED

#include "clsim/random_value/I3CLSimRandomValue.h"

/**
 * @brief A value chosen according to the simplified Liu distribution.
 * (see Pingyu Liu, "A new phase function approximating to mie
 * scattering for radiative transport equations", Phys. Med. Biol.,
 * 39:1025, 1994)
 *
 * The distribution looks like this:
 *
 * p(x) propto (1 + x)^alpha
 *
 * where x=cos(theta), alpha=2g/(1-g) and g=<cos(theta)> [the mean cosine].
 */
static const unsigned i3clsimrandomvaluesimplifiedliu_version_ = 0;

struct I3CLSimRandomValueSimplifiedLiu : public I3CLSimRandomValue
{
public:
    
    I3CLSimRandomValueSimplifiedLiu(double meanCosine);
    virtual ~I3CLSimRandomValueSimplifiedLiu();

    virtual std::size_t NumberOfParameters() const;

    virtual double SampleFromDistribution(const I3RandomServicePtr &random,
                                          const std::vector<double> &parameters) const;

    virtual bool OpenCLFunctionWillOnlyUseASingleRandomNumber() const {return true;}

    virtual std::string GetOpenCLFunction(const std::string &functionName,
                                          const std::string &functionArgs,
                                          const std::string &functionArgsToCall,
                                          const std::string &uniformRandomCall_co,
                                          const std::string &uniformRandomCall_oc
                                          ) const;

    virtual bool CompareTo(const I3CLSimRandomValue &other) const;
    
private:
    I3CLSimRandomValueSimplifiedLiu();

    double meanCosine_;
    
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive & ar, unsigned version);
};


BOOST_CLASS_VERSION(I3CLSimRandomValueSimplifiedLiu, i3clsimrandomvaluesimplifiedliu_version_);

I3_POINTER_TYPEDEFS(I3CLSimRandomValueSimplifiedLiu);

#endif //I3CLSIMRANDOMVALUESIMPLIFIEDLIU_H_INCLUDED
