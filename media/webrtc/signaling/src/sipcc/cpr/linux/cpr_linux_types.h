/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_LINUX_TYPES_H_
#define _CPR_LINUX_TYPES_H_

#include "sys/types.h"
#include "stddef.h"
#include "inttypes.h"

/**
 * @typedef boolean
 *
 * Define boolean as an unsigned byte
 *
 * @note There are differences within TNP header files
 *    @li curses.h:   bool => char
 *    @li types.h:    boolean_t => enum
 *    @li dki_lock.h: bool_t => int
 */
typedef uint8_t boolean;

/*
 * Define size_t
 *    defined in numerous header files
 */
/* DONE (sys/types.h => unsigned int) */

/*
 * Define ssize_t
 */
/* DONE (sys/types.h => int) */

/*
 * Define MIN/MAX
 *    defined in param.h
 *
 * The GNU versions of the MAX and MIN macros do two things better than
 * the old versions:
 * 1. they are more optimal as they only evaluate a & b once by creating a
 *    a variable of each type on the local stack.
 * 2. they fix potential errors due to side-effects where a and b were
 *    evaluated twice, i.e. MIN(i++,j++)
 *
 * @note b could be cast to a's type, to help with usage where the code
 *       compares signed and unsigned types.
 */
#ifndef MIN
#ifdef __GNUC__
#define MIN(a,b)  ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })
#else
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif
#endif

#ifndef MAX
#ifdef __GNUC__
#define MAX(a,b)  ({ typeof(a) _a = (a); typeof(b) _b = (b); _a > _b ? _a : _b; })
#else
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif
#endif


/*
 * Define NULL
 *    defined in numerous header files
 */
/* DONE (stddef.h) */

/**
 * @def NUL
 *
 * Define NUL for string termination
 */
#ifndef NUL
#define NUL '\0'
#endif

/**
 * @def RESTRICT
 *
 * If suppoprting the ISO/IEC 9899:1999 standard,
 * use the '__restrict' keyword
 */
#if defined(_POSIX_C_SOURCE) && defined(__GNUC__)
#define RESTRICT __restrict
#else
#define RESTRICT
#endif

/**
 * @def CONST
 *
 * Define CONST as @c const, if supported
 */
#define CONST const

/**
 * @def INLINE
 *
 * Define the appropriate setting for inlining functions
 */
#ifdef __STRICT_ANSI__
#define INLINE
#else
#define INLINE __inline__
#endif

/**
 * __BEGIN_DECLS and __END_DECLS
 *
 * Define macros for compilation by C++ compiler
 */
#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#else
#define __BEGIN_DECLS
#endif
#endif

#ifndef __END_DECLS
#ifdef __cplusplus
#define __END_DECLS   }
#else
#define __END_DECLS
#endif
#endif

/**
 * Define TRUE/FALSE
 *     defined in several header files
 */
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/*
 * Define offsetof
 */
/* DONE (stddef.h) */

/**
 * @def FIELDOFFSET(struct name, field name)
 *
 * Macro to generate offset from a given field in a structure
 */
#define FIELDOFFSET(struct_name, field_name) (size_t)(&(((struct_name *)0)->field_name))


#endif
