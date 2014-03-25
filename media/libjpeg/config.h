#define VERSION "1.3.1"
#define BUILD "2014-03-22"
#define PACKAGE_NAME "libjpeg-turbo"

/* Need to use Mozilla-specific function inlining. */
#include "mozilla/Attributes.h"
#define INLINE MOZ_ALWAYS_INLINE
