/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
