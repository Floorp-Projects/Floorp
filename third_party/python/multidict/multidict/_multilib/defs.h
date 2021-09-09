#ifndef _MULTIDICT_DEFS_H
#define _MULTIDICT_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

_Py_IDENTIFIER(lower);

/* We link this module statically for convenience.  If compiled as a shared
   library instead, some compilers don't allow addresses of Python objects
   defined in other libraries to be used in static initializers here.  The
   DEFERRED_ADDRESS macro is used to tag the slots where such addresses
   appear; the module init function must fill in the tagged slots at runtime.
   The argument is for documentation -- the macro ignores it.
*/
#define DEFERRED_ADDRESS(ADDR) 0

#ifdef __cplusplus
}
#endif
#endif
