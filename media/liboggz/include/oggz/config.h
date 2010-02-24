#if defined(WIN32) && !defined(__GNUC__)
#include "config_win32.h"
#else
#include "config_gcc.h"
#include "prcpucfg.h"
#ifdef IS_BIG_ENDIAN
#define WORDS_BIGENDIAN
#endif
#endif
#undef DEBUG
