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

#ifndef _CPR_LINUX_ASSERT_H_
#define _CPR_LINUX_ASSERT_H_

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
