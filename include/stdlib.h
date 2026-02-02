#ifdef TARGET_PC
#pragma once
#include_next <stdlib.h>
#else /* !TARGET_PC */
#ifndef _STDLIB_H
#define _STDLIB_H

#ifdef __MWERKS__
#define abs(x) __abs(x)
#else
int abs(int x);
#endif

#endif
#endif
