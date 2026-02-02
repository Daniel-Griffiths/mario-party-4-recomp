#ifndef _REL_EXECUTOR_C_INCLUDED
#define _REL_EXECUTOR_C_INCLUDED
#include "REL/executor.h"

#ifdef TARGET_PC
/* On GC, the linker generates _ctors/_dtors tables.
 * On PC, provide default empty arrays (weak so REL stubs can override). */
__attribute__((weak)) const VoidFunc _ctors[] = { 0 };
__attribute__((weak)) const VoidFunc _dtors[] = { 0 };
#endif

#ifdef TARGET_PC
__attribute__((weak))
#endif
s32 _prolog(void) {
    const VoidFunc* ctors = _ctors;
    while (*ctors != 0) {
        (**ctors)();
        ctors++;
    }
    ObjectSetup();
    return 0;
}

#ifdef TARGET_PC
__attribute__((weak))
#endif
void _epilog(void) {
    const VoidFunc* dtors = _dtors;
    while (*dtors != 0) {
        (**dtors)();
        dtors++;
    }
}
#endif /* _REL_EXECUTOR_C_INCLUDED */
