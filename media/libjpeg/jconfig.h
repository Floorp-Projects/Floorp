/* jconfig.h.  Generated from jconfig.h.in by configure, then manually edited
   for Mozilla. */

/* Export libjpeg v6.2's ABI. */
#define JPEG_LIB_VERSION 62

/* libjpeg-turbo version */
#define LIBJPEG_TURBO_VERSION 2.0.6

/* libjpeg-turbo version in integer form */
#define LIBJPEG_TURBO_VERSION_NUMBER 2000006

/* Support arithmetic encoding */
/* #undef C_ARITH_CODING_SUPPORTED */

/* Support arithmetic decoding */
/* #undef D_ARITH_CODING_SUPPORTED */

/* Support in-memory source/destination managers */
/* #undef MEM_SRCDST_SUPPORTED */

/* Use accelerated SIMD routines. */
#define WITH_SIMD 1

/*
 * Define BITS_IN_JSAMPLE as either
 *   8   for 8-bit sample values (the usual setting)
 *   12  for 12-bit sample values
 * Only 8 and 12 are legal data precisions for lossy JPEG according to the
 * JPEG standard, and the IJG code does not support anything else!
 * We do not support run-time selection of data precision, sorry.
 */
#define BITS_IN_JSAMPLE  8      /* use 8 or 12 */

/* Define to 1 if you have the <locale.h> header file. */
/* #undef HAVE_LOCALE_H */

/* Define to 1 if you have the <stddef.h> header file. */
#define HAVE_STDDEF_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Need to include <sys/types.h> in order to obtain size_t. */
#define NEED_SYS_TYPES_H 1

/* Compiler has <strings.h> rather than standard <string.h>. */
/* #undef NEED_BSD_STRINGS */

/* Compiler supports 'unsigned char'. */
#define HAVE_UNSIGNED_CHAR 1

/* Compiler supports 'unsigned short'. */
#define HAVE_UNSIGNED_SHORT 1

/* Compiler does not support pointers to unspecified structures. */
/* #define INCOMPLETE_TYPES_BROKEN 1 */

/* Broken compiler shifts signed values as an unsigned shift. */
/* #undef RIGHT_SHIFT_IS_UNSIGNED */

/* Define to 1 if type `char' is unsigned and you are not using gcc.  */
#ifndef __CHAR_UNSIGNED__
/* # undef __CHAR_UNSIGNED__ */
#endif

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */
