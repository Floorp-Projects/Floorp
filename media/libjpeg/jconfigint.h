#define VERSION "1.4.0"
#define BUILD "2015-01-07"
#define PACKAGE_NAME "libjpeg-turbo"

/* Need to use Mozilla-specific function inlining. */
#include "mozilla/Attributes.h"
#define INLINE MOZ_ALWAYS_INLINE
