#pragma once

#if defined(ORB_LIB_EXPORT) // inside DLL
#   define ORB_LIB   __declspec(dllexport)
#else // outside DLL
#   define ORB_LIB   __declspec(dllimport)
#endif  // XYZLIBRARY_EXPORT
