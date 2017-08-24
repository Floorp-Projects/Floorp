#include "mozilla/Module.h"

/* Ensure end_kPStaticModules is at the end of the .kPStaticModules section
 * on Windows. Somehow, placing the object last is not enough with PGO/LTCG. */
#ifdef _MSC_VER
/* Sections on Windows are in two parts, separated with $. When linking,
 * sections with the same first part are all grouped, and ordered
 * alphabetically with the second part as sort key. */
#  pragma section(".kPStaticModules$Z", read)
#  undef NSMODULE_SECTION
#  define NSMODULE_SECTION __declspec(allocate(".kPStaticModules$Z"), dllexport)
#endif
/* This could be null, but this needs a dummy value to ensure it actually ends
 * up in the same section as other NSMODULE_DEFNs, instead of being moved to a
 * separate readonly section. */
NSMODULE_DEFN(end_kPStaticModules) = (mozilla::Module*)&NSMODULE_NAME(end_kPStaticModules);
