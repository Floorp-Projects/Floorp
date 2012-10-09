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
 * sstrncpy
 *
 * This is Cisco's *safe* version of strncpy.  The string will always
 * be NUL terminated (which is not ANSI compliant).
 *
 * Parameters: s1  - first string
 *             s2  - second string
 *             max - maximum length in octets to concat.
 *
 * Return:     Pointer to the *end* of the string
 *
 * Remarks:    Modified to be explicitly safe for all inputs.
 *             Also return the number of characters copied excluding the
 *             NUL terminator vs. the original string s1.  This simplifies
 *             code where sstrncat functions follow.
 */
unsigned long
sstrncpy (char *dst, const char *src, unsigned long max)
{
    unsigned long cnt = 0;

    if (dst == NULL) {
        return 0;
    }

    if (src) {
        while ((max-- > 1) && (*src)) {
            *dst = *src;
            dst++;
            src++;
            cnt++;
        }
    }

#if defined(CPR_SSTRNCPY_PAD)
    /*
     * To be equivalent to the TI compiler version
     * v2.01, SSTRNCPY_PAD needs to be defined
     */
    while (max-- > 1) {
        *dst = '\0';
        dst++;
    }
#endif
    *dst = '\0';

    return cnt;
}

/**
 * sstrncat
 *
 * This is Cisco's *safe* version of strncat.  The string will always
 * be NUL terminated (which is not ANSI compliant).
 *
 * Parameters: s1  - first string
 *             s2  - second string
 *             max - maximum length in octets to concatenate
 *
 * Return:     Pointer to the *end* of the string
 *
 * Remarks:    Modified to be explicitly safe for all inputs.
 *             Also return the end vs. the beginning of the string s1
 *             which is useful for multiple sstrncat calls.
 */
char *
sstrncat (char *s1, const char *s2, unsigned long max)
{
    if (s1 == NULL)
        return (char *) NULL;

    while (*s1)
        s1++;

    if (s2) {
        while ((max-- > 1) && (*s2)) {
            *s1 = *s2;
            s1++;
            s2++;
        }
    }
    *s1 = '\0';

    return s1;
}
