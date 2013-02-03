#include "mozilla/Assertions.h"

#include "nsError.h"

// No reason to pull in Assertions.h for every single file that includes
// nsError.h, so let's put this in its own .cpp file instead of in the .h.
MOZ_STATIC_ASSERT(((nsresult)0) < ((nsresult)-1),
                  "nsresult must be an unsigned type");
MOZ_STATIC_ASSERT(sizeof(nsresult) == sizeof(uint32_t),
                  "nsresult must be 32 bits");
