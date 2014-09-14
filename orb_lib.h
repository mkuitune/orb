/** \file orb_lib.h
\author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
MIT licence.
*/

#pragma once

#if defined(ORB_LIB_EXPORT) // inside DLL
#   define ORB_LIB   __declspec(dllexport)
#else // outside DLL
#   define ORB_LIB   __declspec(dllimport)
#endif  // XYZLIBRARY_EXPORT
