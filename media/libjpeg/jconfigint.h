/* libjpeg-turbo build number */
#define BUILD  "20240508"

/* How to hide global symbols. */
#define HIDDEN __attribute__((visibility("hidden")))

/* Need to use Mozilla-specific function inlining. */
#include "mozilla/Attributes.h"
#define INLINE MOZ_ALWAYS_INLINE

/* How to obtain thread-local storage */
#if defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL __thread
#endif

/* Define to the full name of this package. */
#define PACKAGE_NAME  "libjpeg-turbo"

/* Version number of package */
#define VERSION  "3.0.3"

/* The size of `size_t', as computed by sizeof. */
#ifdef HAVE_64BIT_BUILD
#define SIZEOF_SIZE_T 8
#else
#define SIZEOF_SIZE_T 4
#endif

/* Define if your compiler has __builtin_ctzl() and sizeof(unsigned long) == sizeof(size_t). */
#ifndef _MSC_VER
#define HAVE_BUILTIN_CTZL 1
#endif

/* Define to 1 if you have the <intrin.h> header file. */
#ifdef _MSC_VER
#define HAVE_INTRIN_H 1
#endif

#if defined(_MSC_VER) && defined(HAVE_INTRIN_H)
#if (SIZEOF_SIZE_T == 8)
#define HAVE_BITSCANFORWARD64
#elif (SIZEOF_SIZE_T == 4)
#define HAVE_BITSCANFORWARD
#endif
#endif

#if defined(__has_attribute)
#if __has_attribute(fallthrough)
#define FALLTHROUGH  __attribute__((fallthrough));
#else
#define FALLTHROUGH
#endif
#else
#define FALLTHROUGH
#endif

/*
 * Define BITS_IN_JSAMPLE as either
 *   8   for 8-bit sample values (the usual setting)
 *   12  for 12-bit sample values
 * Only 8 and 12 are legal data precisions for lossy JPEG according to the
 * JPEG standard, and the IJG code does not support anything else!
 */

#ifndef BITS_IN_JSAMPLE
#define BITS_IN_JSAMPLE  8      /* use 8 or 12 */
#endif

#undef C_ARITH_CODING_SUPPORTED
#undef D_ARITH_CODING_SUPPORTED
#undef WITH_SIMD

#if BITS_IN_JSAMPLE == 8

/* Support arithmetic encoding */
/* #undef C_ARITH_CODING_SUPPORTED */

/* Support arithmetic decoding */
/* #undef D_ARITH_CODING_SUPPORTED */

/* Use accelerated SIMD routines. */
#if defined(__sparc__)
#undef  WITH_SIMD
#else
#define WITH_SIMD 1
#endif

#endif
