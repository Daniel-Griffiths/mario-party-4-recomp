#ifndef PC_CONFIG_H
#define PC_CONFIG_H

/* Master configuration for the Mario Party 4 PC port */

#include <stdint.h>
#include <string.h>

/* ---- Byte-swap utilities (GC data is big-endian) ---- */
#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define BE16(x) OSSwapBigToHostInt16(x)
#define BE32(x) OSSwapBigToHostInt32(x)
#else
#include <endian.h>
#define BE16(x) be16toh(x)
#define BE32(x) be32toh(x)
#endif

static inline float BEF32(float f) {
    union { float f; uint32_t u; } conv;
    conv.f = f;
    conv.u = BE32(conv.u);
    return conv.f;
}

/* ---- Path to extracted game files ---- */
#ifndef MP4_DATA_PATH
#define MP4_DATA_PATH "orig/GMPE01_00/files/"
#endif

/* ---- Screen dimensions ---- */
#define PC_SCREEN_WIDTH  640
#define PC_SCREEN_HEIGHT 480

/* ---- GC hardware clock constants (for timer macros) ---- */
#define PC_BUS_CLOCK  162000000u   /* 162 MHz */
#define PC_CORE_CLOCK 486000000u   /* 486 MHz */

#endif /* PC_CONFIG_H */
