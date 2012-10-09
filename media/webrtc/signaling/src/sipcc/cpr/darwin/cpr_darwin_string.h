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

#ifndef _CPR_DARWIN_STRING_H_
#define _CPR_DARWIN_STRING_H_

#include <string.h>
#include <ctype.h>

/**
 * cpr_strdup
 *
 * @brief The CPR wrapper for strdup

 * The cpr_strdup shall return a pointer to a new string, which is a duplicate
 * of the string pointed to by "str" argument. A null pointer is returned if the
 * new string cannot be created.
 *
 * @param[in] str  - The string that needs to be duplicated
 *
 * @return The duplicated string or NULL in case of no memory
 *
 */
char * 
cpr_strdup(const char *str);

/**
 * strcasestr
 *
 * @brief The same as strstr, but ignores case
 *
 * The strcasestr performs the strstr function, but ignores the case. 
 * This function shall locate the first occurrence in the string 
 * pointed to by s1 of the sequence of bytes (excluding the terminating 
 * null byte) in the string pointed to by s2.
 *
 * @param[in] s1  - The input string 
 * @param[in] s2  - The pattern to be matched
 *
 * @return A pointer to the first occurrence of string s2 found
 *           in string s1 or NULL if not found.  If s2 is an empty
 *           string then s1 is returned.
 */
char *
strcasestr(const char *s1, const char *s2);


#endif
