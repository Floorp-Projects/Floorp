/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_ANDROID_ASSERT_H_
#define _CPR_ANDROID_ASSERT_H_

#include "assert.h"

/*--------------------------------------
 *
 * Macros
 *
 */

/**
 * CPR assert macro which calls cpr_assert_msg instead of abort
 *
 * The macro is dependent on the setting of FILE_ID which is used
 * to override the __FILE__ setting.  For certain compilers, i.e.
 * read 'Diab 4.4b', the __FILE__ is set to a Windows type path
 * name which can contain backslashes that can cause odd output.
 */
#ifdef FILE_ID
#define cpr_assert(expr) \
    ((expr) ? (void)0 : cpr_assert_msg(FILE_ID, __LINE__, #expr))
#else
#define cpr_assert(expr) \
    ((expr) ? (void)0 : cpr_assert_msg(__FILE__, __LINE__, #expr))
#endif

#define cpr_assert_debug(expr)

/*
 * A side note if somehow concerned about performance.
 *
 * This method will pre-render the string via the compiler,
 * but will use more space due to larger strings.  Basically,
 * good for speed and bad for memory.
 *
 * This is coded mostly as an example so if performance was an issue
 * that the asserts could be low impact.
 *
 * #define cpr_assert_debug(expr) \
 *   ((expr) ? (void)0 : cpr_assert_msg( \
 *             __FILE__ ": line " __LINE__ ": assertion failed: " #expr))
 *
 * Note that this is not allowed when using __STRING_ANSI__
 */

#define cpr_assert_debug_rtn(expr)

/*--------------------------------------
 *
 * Structures
 *
 */

/**
 * CPR assert modes of operation
 */
typedef enum {
    CPR_ASSERT_MODE_NONE,            /**< Off, no message ouput          */
    CPR_ASSERT_MODE_WARNING_LIMITED, /**< Warnings to syslog are limited */
    CPR_ASSERT_MODE_WARNING_ALL,     /**< All warnings sent to syslog    */
    CPR_ASSERT_MODE_ABORT            /**< Assert failure will call abort */
} cpr_assert_mode_e;


/*--------------------------------------
 *
 * Globals
 *
 */
extern uint32_t cpr_assert_count;

/*--------------------------------------
 *
 * Prototypes
 *
 */
void
cpr_assert_msg(const char *file, const int line, const char *expression);

#endif
