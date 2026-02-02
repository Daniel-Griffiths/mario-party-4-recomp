/*
 * PPC EABI Runtime stubs
 * Replaces src/Runtime.PPCEABI.H/ functions
 */
#include "dolphin/types.h"

/* C++ exception init (not needed on PC) */
void __init_cpp_exceptions(void) {}
void __fini_cpp_exceptions(void) {}

/* OSFastCast - on PPC this sets GQR registers for fast float conversion.
 * On PC, standard C casts work fine. This is declared as static inline
 * in the header but game code also calls it directly. */
void OSInitFastCast(void) {}

/* Fast cast helper functions - standard C casts on PC */
s16 __OSf32tos16(f32 inF) { return (s16)inF; }
void OSf32tos16(f32 *f, s16 *out) { *out = (s16)*f; }
u8 __OSf32tou8(f32 inF) { return (u8)inF; }
void OSf32tou8(f32 *f, u8 *out) { *out = (u8)*f; }
s8 __OSf32tos8(f32 inF) { return (s8)inF; }
void OSf32tos8(f32 *f, s8 *out) { *out = (s8)*f; }
u16 __OSf32tou16(f32 inF) { return (u16)inF; }
void OSf32tou16(f32 *f, u16 *out) { *out = (u16)*f; }
f32 __OSs8tof32(const s8 *arg) { return (f32)*arg; }
void OSs8tof32(const s8 *in, f32 *out) { *out = (f32)*in; }
f32 __OSs16tof32(const s16 *arg) { return (f32)*arg; }
void OSs16tof32(const s16 *in, f32 *out) { *out = (f32)*in; }
f32 __OSu8tof32(const u8 *arg) { return (f32)*arg; }
void OSu8tof32(const u8 *in, f32 *out) { *out = (f32)*in; }
f32 __OSu16tof32(const u16 *arg) { return (f32)*arg; }
void OSu16tof32(const u16 *in, f32 *out) { *out = (f32)*in; }

/* DEMOStats stubs */
void DEMOUpdateStats(int flag) { (void)flag; }
void DEMOPrintStats(void) {}

/* Soft reset check */
s32 HuSoftResetButtonCheck(void) { return 0; }
