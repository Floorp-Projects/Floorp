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

#ifndef _CPR_STRINGS_H_
#define _CPR_STRINGS_H_

#include "cpr_types.h"

__BEGIN_DECLS

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_strings.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_strings.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_strings.h"
#endif

#ifdef CPR_USE_OS_STRCASECMP
/* Use standard library types */
#define cpr_strcasecmp  strcasecmp
#define cpr_strncasecmp strncasecmp
#else
/* Prototypes */
/**
 * cpr_strcasecmp
 *
 * @brief The CPR wrapper for strcasecmp
 *
 * The cpr_strcasecmp performs case insensitive string comparison of the "s1"
 * and the "s2" strings.
 *
 * @param[in] s1  - The first string 
 * @param[in] s2  - The second string 
 *
 * @return integer <,=,> 0 depending on whether s1 is <,=,> s2
 */
int cpr_strcasecmp(const char *s1, const char *s2);

/**
 * cpr_strncasecmp
 *
 * @brief The CPR wrapper for strncasecmp
 *
 * The cpr_strncasecmp performs case insensitive string comparison for specific
 * length "len".
 *
 * @param[in] s1  - The first string 
 * @param[in] s2  - The second string 
 * @param[in] len  - The length to be compared
 *
 * @return integer <,=,> 0 depending on whether s1 is <,=,> s2
 */
int cpr_strncasecmp(const char *s1, const char *s2, size_t len);
#endif

__END_DECLS

#endif
