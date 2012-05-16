#if defined(VPX_X86_ASM)

#if defined(_WIN64)
/* 64 bit Windows */
#include "vpx_config_x86_64-win64-vs8.h"

#elif defined(_WIN32)
/* 32 bit Windows, MSVC. */
#include "vpx_config_x86-win32-vs8.h"

#elif defined(__APPLE__) && defined(__x86_64__)
/* 64 bit MacOS. */
#include "vpx_config_x86_64-darwin9-gcc.h"

#elif defined(__APPLE__) && defined(__i386__)
/* 32 bit MacOS. */
#include "vpx_config_x86-darwin9-gcc.h"

#elif defined(__linux__) && defined(__i386__)
/* 32 bit Linux. */
#include "vpx_config_x86-linux-gcc.h"

#elif defined(__linux__) && defined(__x86_64__)
/* 64 bit Linux. */
#include "vpx_config_x86_64-linux-gcc.h"

#elif defined(__sun) && defined(__i386)
/* 32 bit Solaris. */
#include "vpx_config_x86-linux-gcc.h"

#elif defined(__sun) && defined(__x86_64)
/* 64 bit Solaris. */
#include "vpx_config_x86_64-linux-gcc.h"

#else
#error VPX_X86_ASM is defined, but assembly not supported on this platform!
#endif

#elif defined(VPX_ARM_ASM)

#if defined(__linux__) && defined(__GNUC__)
/* ARM Linux */
#include "vpx_config_arm-linux-gcc.h"

#else
#error VPX_ARM_ASM is defined, but assembly not supported on this platform!
#endif

#else
/* Assume generic GNU/GCC configuration. */
#include "vpx_config_generic-gnu.h"
#endif

/* Control error-concealment support using our own #define rather than
   hard-coding it. */
#if defined(MOZ_VP8_ERROR_CONCEALMENT)
#undef CONFIG_ERROR_CONCEALMENT
#define CONFIG_ERROR_CONCEALMENT 1
#endif

/* Control encoder support using our own #define rather than hard-coding it. */
#if defined(MOZ_VP8_ENCODER)
#undef CONFIG_VP8_ENCODER
#undef CONFIG_ENCODERS
#undef CONFIG_MULTI_RES_ENCODING
#define CONFIG_VP8_ENCODER 1
#define CONFIG_ENCODERS 1
#define CONFIG_MULTI_RES_ENCODING 1
#endif
