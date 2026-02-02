#ifdef TARGET_PC
/* On PC, this stub must not shadow the real system stdint.h.
 * We use #include_next to forward to the real one. */
#pragma once
#include_next <stdint.h>
#else /* !TARGET_PC */

#ifndef _STDINT_H_
#define _STDINT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long int uintptr_t;

#ifdef __cplusplus
}
#endif

#endif /* _STDINT_H_ */
#endif /* TARGET_PC */