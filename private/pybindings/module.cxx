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
 * @file module.cxx
 * @version $Revision$
 * @date $Date$
 * @author Claudio Kopper
 */

#include <icetray/I3FrameObject.h>
#include <icetray/load_project.h>

using namespace boost::python;
namespace bp = boost::python;
#include <boost/preprocessor.hpp>

#define REGISTER_THESE_THINGS                       \
    (I3CLSimStep)(I3CLSimPhoton)                    \
    (I3CLSimPhotonHistory)                          \
    (I3CLSimFunction)                     \
    (I3CLSimMediumProperties)(I3CLSimRandomValue)   \
    (I3CLSimLightSourceToStepConverter)             \
    (I3CLSimStepToPhotonConverter)                  \
    (I3CLSimSimpleGeometry)                         \
    (I3Photon)(I3CLSimLightSourceParameterization)  \
    (I3CLSimTester)(I3ModuleHelper)                 \
    (I3CLSimLightSourceToStepConverterUtils)        \
    (I3CLSimEventStatistics)                        \
    (I3CLSimPMTPhotonSimulator)                     \
    (I3Converters)(I3CLSimOpenCLDevice)             \
    (I3CLSimLightSource)(I3CLSimSpectrumTable)      \
    (I3CLSimFlasherPulse)(I3ShadowedPhotonRemover)  \
    (I3ExtraGeometryItem)

#define I3_REGISTRATION_FN_DECL(r, data, t) void BOOST_PP_CAT(register_,t)();
#define I3_REGISTER(r, data, t) BOOST_PP_CAT(register_,t)();
BOOST_PP_SEQ_FOR_EACH(I3_REGISTRATION_FN_DECL, ~, REGISTER_THESE_THINGS)

BOOST_PYTHON_MODULE(clsim)
{
  load_project("libclsim", false);

  BOOST_PP_SEQ_FOR_EACH(I3_REGISTER, ~, REGISTER_THESE_THINGS);
}

