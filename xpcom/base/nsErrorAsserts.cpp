#include "nsError.h"
#include "mozilla/Assertions.h"

// No reason to pull in Assertions.h for every single file that includes
// nsError.h, so let's put this in its own .cpp file instead of in the .h.
MOZ_STATIC_ASSERT(sizeof(nsresult) == sizeof(uint32_t),
                  "nsresult must be 32 bits");
