#include "prcpucfg.h"

/* We don't use configure here, so use nspr's #define instead */

#ifdef IS_BIG_ENDIAN
#define WORDS_BIGENDIAN 1
#else
#undef WORDS_BIGENDIAN
#endif
