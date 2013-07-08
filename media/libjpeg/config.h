#define VERSION "1.3.0"
#define BUILD "2013-05-25"
#define PACKAGE_NAME "libjpeg-turbo"

/* Need to use Mozilla-specific function inlining. */
#include "mozilla/Attributes.h"
#define INLINE MOZ_ALWAYS_INLINE
