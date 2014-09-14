/** \file allocator.cpp Memory allocator 
    \author Mikko Kuitunen (mikko <dot> kuitunen <at> iki <dot> fi)
*/
#include "allocators.h"

namespace {
    const size_t g_alignment = 16;
}

unsigned char* aligned_alloc(size_t size)
{
    unsigned char* p = new unsigned char[size + g_alignment];
    size_t offset = g_alignment - (size_t(p) & (g_alignment - 1));

    unsigned char* result = p + offset;
    result[-1] = (unsigned char)offset;

    return result;
}

void aligned_free(void* p)
{
    if (p)
    {
        unsigned char* mem = (unsigned char*)p;
        mem = mem - mem[-1];
        delete [] mem;
    }
}
