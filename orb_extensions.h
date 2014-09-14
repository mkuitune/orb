/** \file orb_extensions.h
    \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
    MIT licence.
*/
#pragma once

#include "orb.h"

namespace orb{
/* Add system callbacks to orb - file io. */
    ORB_LIB void load_orb_unsafe_extensions(orb::Orb& m);
}

