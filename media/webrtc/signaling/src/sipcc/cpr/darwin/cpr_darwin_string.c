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


#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "cpr_strings.h"


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
cpr_strdup (const char *str)
{
    char *dup;
    size_t len;

    if (!str) {
        return (char *) NULL;
    }

    len = strlen(str);
    if (len == 0) {
        return (char *) NULL;
    }
    len++;

    dup = cpr_malloc(len * sizeof(char));
    if (!dup) {
        return (char *) NULL;
    }
    (void) memcpy(dup, str, len);
    return dup;
}


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
#ifndef CPR_USE_OS_STRCASECMP
int
cpr_strcasecmp (const char *s1, const char *s2)
{
    const unsigned char *us1 = (const unsigned char *) s1;
    const unsigned char *us2 = (const unsigned char *) s2;

    /* No match if only one ptr is NULL */
    if ((!s1 && s2) || (s1 && !s2)) {
        /*
         * If one of these is NULL it will be the lesser of the two
         * values and therefore we'll get the proper sign in the int
         */
        return (int) (s1 - s2);
    }

    /* Match if both ptrs the same (e.g. NULL) */
    if (s1 == s2)
        return 0;

    while (*us1 != '\0' && *us2 != '\0' && toupper(*us1) == toupper(*us2)) {
        us1++;
        us2++;
    }

    return (toupper(*us1) - toupper(*us2));
}

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
int
cpr_strncasecmp (const char *s1, const char *s2, size_t len)
{
    const unsigned char *us1 = (const unsigned char *) s1;
    const unsigned char *us2 = (const unsigned char *) s2;

    /* No match if only one ptr is NULL */
    if ((!s1 && s2) || (s1 && !s2))
        return ((int) (s1 - s2));

    if ((len == 0) || (s1 == s2))
        return 0;

    while (len-- > 0 && toupper(*us1) == toupper(*us2)) {
        if (len == 0 || *us1 == '\0' || *us2 == '\0')
            break;
        us1++;
        us2++;
    }

    return (toupper(*us1) - toupper(*us2));
}
#endif

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
strcasestr (const char *s1, const char *s2)
{
    unsigned int i;

    if (!s1)
        return (char *) NULL;

    if (!s2 || (s1 == s2) || (*s2 == '\0'))
        return (char *) s1;

    while (*s1) {
        i = 0;
        do {
            if (s2[i] == '\0')
                return (char *) s1;
            if (s1[i] == '\0')
                return (char *) NULL;
            if (toupper(s1[i]) != toupper(s2[i]))
                break;
            i++;
        } while (1);
        s1++;
    }

    return (char *) NULL;
}

