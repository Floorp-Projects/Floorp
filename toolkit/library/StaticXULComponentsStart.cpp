#include "mozilla/Module.h"

/* This could be null, but this needs a dummy value to ensure it actually ends
 * up in the same section as other NSMODULE_DEFNs, instead of being moved to a
 * separate readonly section. */
NSMODULE_DEFN(start_kPStaticModules) = (mozilla::Module*)&NSMODULE_NAME(start_kPStaticModules);
