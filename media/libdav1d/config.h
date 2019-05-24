#pragma once

// Set platform
#if defined(__aarch64__) || defined(_M_ARM64)
#  define ARCH_AARCH64 1
#else
#  define ARCH_AARCH64 0
#endif

#if defined(__arm__) || defined(_M_ARM)
#  define ARCH_ARM 1
#else
#  define ARCH_ARM 0
#endif

#if defined(__i386__) || defined(_M_IX86)
#  define ARCH_X86_32 1
#else
#  define ARCH_X86_32 0
#endif

#if defined(__x86_64__) || defined(_M_X64)
#  define ARCH_X86_64 1
#else
#  define ARCH_X86_64 0
#endif

#if ARCH_X86_32 == 1 || ARCH_X86_64 == 1
#  define ARCH_X86 1
#else
#  define ARCH_X86 0
#endif

// Set both bitdepeth in every case
#define CONFIG_16BPC 1
#define CONFIG_8BPC 1

// Enable asm
#if defined(MOZ_DAV1D_ASM)
#  define HAVE_ASM 1
#else
#  define HAVE_ASM 0
#endif

#if ARCH_AARCH64 == 1
// Those values are copied from the auto generated
// config file produced by stand alone dav1d build.
#define HAVE_AS_FUNC 0
#define HAVE_GETAUXVAL 1
#define PIC 3
#endif

// The following macros are defined from autoconf
// according to the system configuration.
// (HAVE_MEMALIGN | HAVE_ALIGNED_MALLOC | HAVE_POSIX_MEMALIGN)
// HAVE_UNISTD_H

// Important when asm is enabled
#if defined(__APPLE__) || (ARCH_x86_32 == 1 && defined(_WIN32))
#  define PREFIX 1
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
// _WIN32_WINNT 0x0601 is set in global macros
#  define UNICODE 1
#  define _UNICODE 1
#  define __USE_MINGW_ANSI_STDIO 1
#endif
