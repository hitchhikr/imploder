// =================================================================
#include "endian.h"

// =================================================================
uint16_t swap_uint16(uint16_t val)
{
#if defined(AHP_LITTLE_ENDIAN)
    return (val << 8) | (val >> 8);
#else
    return val;
#endif
}

// =================================================================
uint32_t swap_uint32(uint32_t val)
{
#if defined(AHP_LITTLE_ENDIAN)
    return ((val & 0x000000ff) << 24) |
           ((val & 0x0000ff00) << 8) |
           ((val & 0x00ff0000) >> 8) |
           ((val & 0xff000000) >> 24);
#else
    return val;
#endif
}

