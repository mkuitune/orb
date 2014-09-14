/** \file allocator.h Memory allocators.
    \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
    MIT license.
*/
#pragma once


#include <cstring>

/** Return memory aligned to 16 bytes. Must be freed using alignedFree */
unsigned char* aligned_alloc(size_t size);
void aligned_free(void* p);
