//
//   Copyright (c) 2011  Claudio Kopper
//   
//   $Id$
//
//   This file is part of the IceTray module clsim
//
//   this file is free software; you can redistribute it and/or modify
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

#include <clsim/I3HoleIceSimulator.h>

#include <boost/preprocessor/seq.hpp>
#include <icetray/python/std_vector_indexing_suite.hpp>

using namespace boost::python;
namespace bp = boost::python;


void register_I3HoleIceSimulator()
{
    {
        bp::scope I3HoleIceSimulator_scope = 
        bp::class_<I3HoleIceSimulator, boost::shared_ptr<I3HoleIceSimulator>, boost::noncopyable>
        ("I3HoleIceSimulator", 
         bp::init<I3RandomServicePtr, double, double, I3CLSimMediumPropertiesConstPtr, I3CLSimWlenDependentValueConstPtr, I3CLSimWlenDependentValueConstPtr>
         (
          (
           bp::arg("random"),
           bp::arg("DOMRadius"),
           bp::arg("holeRadius"),
           bp::arg("mediumProperties"),
           bp::arg("holeIceAbsorptionLength"),
           bp::arg("holeIceScatteringLength")
          )
         )
        )

        .def("TrackPhoton", &I3HoleIceSimulator::TrackPhoton)
        ;
    }
    
    bp::implicitly_convertible<shared_ptr<I3HoleIceSimulator>, shared_ptr<const I3HoleIceSimulator> >();
}