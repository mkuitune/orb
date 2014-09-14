/** \file masp_extensions.h
    \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
*/
#pragma once

#include "masp.h"

namespace orb{
/* Add system callbacks to masp - file io. */
    ORB_LIB void load_masp_unsafe_extensions(orb::Masp& m);
}

