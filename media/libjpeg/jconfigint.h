#define VERSION "1.5.2"
#define BUILD "2017-07-07"
#define PACKAGE_NAME "libjpeg-turbo"

/* Need to use Mozilla-specific function inlining. */
#include "mozilla/Attributes.h"
#define INLINE MOZ_ALWAYS_INLINE
