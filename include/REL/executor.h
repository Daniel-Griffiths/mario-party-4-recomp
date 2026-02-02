#ifndef REL_EXECUTOR_H
#define REL_EXECUTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dolphin/types.h"

typedef void (*VoidFunc)(void);

#ifdef TARGET_PC
/* On PC, each REL module is compiled with -DDLL_ID=<name>.
 * We prefix _prolog/_epilog/ObjectSetup/_ctors/_dtors with the DLL_ID
 * to avoid symbol collisions when linking all modules statically. */
#define _PC_PASTE(a, b) a##b
#define _PC_PASTE2(a, b) _PC_PASTE(a, b)
#define _prolog    _PC_PASTE2(DLL_ID, __prolog)
#define _epilog    _PC_PASTE2(DLL_ID, __epilog)
#define ObjectSetup _PC_PASTE2(DLL_ID, __ObjectSetup)
#define ModuleEpilog _PC_PASTE2(DLL_ID, __ModuleEpilog)
#define _ctors     _PC_PASTE2(DLL_ID, __ctors)
#define _dtors     _PC_PASTE2(DLL_ID, __dtors)
#endif

#ifdef TARGET_PC
#define _REL_EXPORT __attribute__((visibility("default")))
#else
#define _REL_EXPORT
#endif

_REL_EXPORT extern s32 _prolog();
_REL_EXPORT extern void _epilog();

_REL_EXPORT extern const VoidFunc _ctors[];
_REL_EXPORT extern const VoidFunc _dtors[];

_REL_EXPORT extern void ObjectSetup(void);
_REL_EXPORT extern void ModuleEpilog(void);

#ifdef __cplusplus
}
#endif

#endif /* REL_EXECUTOR_H */
