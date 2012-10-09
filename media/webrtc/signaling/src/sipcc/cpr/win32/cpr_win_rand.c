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

#include "cpr.h"


/*
 * cpr_crypto_rand
 *
 * Generate crypto graphically random number for a desired length.
 * On WIN32 platform, there is no security support, for now generate
 * enough random number of bytes using normal "rand()" function.
 *
 * Parameters: 
 *     buf  - pointer to the buffer to store the result of random
 *            bytes requested. 
 *     len  - pointer to the length of the desired random bytes. 
 *            When calling the function, the integer's value 
 *            should be set to the desired number of random 
 *            bytes ('buf' should be of at least this size).
 *            upon success, its value will be set to the
 *            actual number of random bytes being returned.
 *            (realistically, there is a maximum number of 
 *            random bytes that can be returned at a time.
 *            if the caller request more than that, the
 *            'len' will indicate how many bytes are actually being
 *            returned) on failure, its value will be set to 0.
 *
 * Return Value: 
 *     1 - success.
 *     0 - fail. 
 */
int cpr_crypto_rand (uint8_t *buf, int *len)
{
#ifdef CPR_WIN_RAND_DEBUG
    /*
     * This is for debug only.
     */
    int      i, total_bytes;

    if ((buf == NULL) || (len == NULL)) { 
        /* Invalid parameters */
        return (0);
    }

    total_bytes = *len;
    for (i = 0, i < total_bytes ; i++) {
        *buf = rand() & 0xff;
        buf++;
    }
    return (1);

#else
    /*
     * No crypto. random support on WIN32
     */
    if (len != NULL) {
        /* length pointer is ok, set it to zero to indicate failure */
        *len = 0;
    }
    return (0);

#endif
}
