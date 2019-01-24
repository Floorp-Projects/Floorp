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
#if (ARCH_x86_32 == 1 || ARCH_X86_64 == 1) && defined(__linux__) && \
    !defined(__ANDROID__)
#  define HAVE_ASM 1
#else
#  define HAVE_ASM 0
#endif

// Set memory aligment
#if defined(__ANDROID__) && (ARCH_ARM == 1 || ARCH_X86_32 == 1)
#  define HAVE_MEMALIGN 1
#elif ARCH_X86_64 == 1 && (defined(_WIN32) || defined(__CYGWIN__)) && \
    defined(MOZ_ASAN)
#  define HAVE_ALIGNED_MALLOC 1
#else
#  define HAVE_POSIX_MEMALIGN 1
#endif

// unistd.h is used by tools, which we do not
// build, so we do not really care.
#define HAVE_UNISTD_H 1

// Important when asm is enabled
#if defined(__APPLE__)
#  define PREFIX 1
#endif

#if (ARCH_x86_32 == 1 || ARCH_X86_64 == 1) && defined(__linux__) && \
    !defined(__ANDROID__)
#  define STACK_ALIGNMENT 16
#else
#  define STACK_ALIGNMENT 32
#endif
