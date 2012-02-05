//
//   Copyright (c) 2012  Claudio Kopper
//   
//   $Id$
//
//   This file is free software; you can redistribute it and/or modify
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

#include <sstream>

#include <icetray/I3Units.h>

#include <clsim/I3CLSimFlasherPulse.h>
#include <boost/preprocessor/seq.hpp>
#include <icetray/python/std_vector_indexing_suite.hpp>

namespace bp = boost::python;

#define ENUM_DEF(r,data,T) .value(BOOST_PP_STRINGIZE(T), data::T)


void register_I3CLSimFlasherPulse()
{
    {
        bp::scope I3CLSimFlasherPulse_scope = 
        bp::class_<I3CLSimFlasherPulse, boost::shared_ptr<I3CLSimFlasherPulse> >
        ("I3CLSimFlasherPulse")
        
        .def("GetType", &I3CLSimFlasherPulse::GetType)
        .def("SetType", &I3CLSimFlasherPulse::SetType)
        .add_property("type", &I3CLSimFlasherPulse::GetType, &I3CLSimFlasherPulse::SetType)

        .def("GetPos", (const I3Position& (I3CLSimFlasherPulse::*)()) &I3CLSimFlasherPulse::GetPos, bp::return_internal_reference<1>() )
        .def("SetPos", (void (I3CLSimFlasherPulse::*)(const I3Position&)) &I3CLSimFlasherPulse::SetPos)
        .def("GetDir", (const I3Direction& (I3CLSimFlasherPulse::*)()) &I3CLSimFlasherPulse::GetDir, bp::return_internal_reference<1>() )
        .def("SetDir", (void (I3CLSimFlasherPulse::*)(const I3Direction&)) &I3CLSimFlasherPulse::SetDir)

        .add_property("pos", bp::make_function( (const I3Position& (I3CLSimFlasherPulse::*)()) &I3CLSimFlasherPulse::GetPos, bp::return_internal_reference<1>() ),
					  (void (I3CLSimFlasherPulse::*)(const I3Position&)) &I3CLSimFlasherPulse::SetPos ) 
        .add_property("dir", bp::make_function( (const I3Direction& (I3CLSimFlasherPulse::*)()) &I3CLSimFlasherPulse::GetDir, bp::return_internal_reference<1>() ),
					  (void (I3CLSimFlasherPulse::*)(const I3Direction&)) &I3CLSimFlasherPulse::SetDir ) 

        .def("GetTime", &I3CLSimFlasherPulse::GetTime)
        .def("SetTime", &I3CLSimFlasherPulse::SetTime)
        .add_property("time", &I3CLSimFlasherPulse::GetTime, &I3CLSimFlasherPulse::SetTime)

        .def("GetNumberOfPhotonsNoBias", &I3CLSimFlasherPulse::GetNumberOfPhotonsNoBias)
        .def("SetNumberOfPhotonsNoBias", &I3CLSimFlasherPulse::SetNumberOfPhotonsNoBias)
        .add_property("numberOfPhotonsNoBias", &I3CLSimFlasherPulse::GetNumberOfPhotonsNoBias, &I3CLSimFlasherPulse::SetNumberOfPhotonsNoBias)

        .def("GetPulseWidth", &I3CLSimFlasherPulse::GetPulseWidth)
        .def("SetPulseWidth", &I3CLSimFlasherPulse::SetPulseWidth)
        .add_property("pulseWidth", &I3CLSimFlasherPulse::GetPulseWidth, &I3CLSimFlasherPulse::SetPulseWidth)
        ;
        
        
        bp::enum_<I3CLSimFlasherPulse::FlasherPulseType>("FlasherPulseType")
        BOOST_PP_SEQ_FOR_EACH(ENUM_DEF,I3CLSimFlasherPulse,I3CLSIMFLASHERPULSE_H_I3CLSimFlasherPulse_FlasherPulseType)
        .export_values()
        ;

    }
    
    bp::class_<I3CLSimFlasherPulseSeries, bp::bases<I3FrameObject>, I3CLSimFlasherPulseSeriesPtr>("I3CLSimFlasherPulseSeries")
    .def(bp::std_vector_indexing_suite<I3CLSimFlasherPulseSeries>())
    ;

    bp::implicitly_convertible<shared_ptr<I3CLSimFlasherPulse>, shared_ptr<const I3CLSimFlasherPulse> >();
    register_pointer_conversions<I3CLSimFlasherPulseSeries>();
}
