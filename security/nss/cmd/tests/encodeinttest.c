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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <stdio.h>

#include "secasn1.h"

struct TestCase {
    long value;
    unsigned char data[5];
    unsigned int len;
};

static struct TestCase testCase[] = {
    /* XXX NSS doesn't generate the shortest encoding for negative values. */
#if 0
    { -128, { 0x80 }, 1 },
    { -129, { 0xFF, 0x7F }, 2 },
#endif

    { 0, { 0x00 }, 1 },
    { 127, { 0x7F }, 1 },
    { 128, { 0x00, 0x80 }, 2 },
    { 256, { 0x01, 0x00 }, 2 },
    { 32768, { 0x00, 0x80, 0x00 }, 3 }
};

int main()
{
    PRBool failed = PR_FALSE;
    unsigned int i;
    unsigned int j;

    for (i = 0; i < sizeof(testCase)/sizeof(testCase[0]); i++) {
        SECItem encoded;
        if (SEC_ASN1EncodeInteger(NULL, &encoded, testCase[i].value) == NULL) {
            fprintf(stderr, "SEC_ASN1EncodeInteger failed\n");
            failed = PR_TRUE;
            continue;
        }
        if (encoded.len != testCase[i].len ||
            memcmp(encoded.data, testCase[i].data, encoded.len) != 0) {
            fprintf(stderr, "Encoding of %ld is incorrect:",
                    testCase[i].value);
            for (j = 0; j < encoded.len; j++) {
                fprintf(stderr, " 0x%02X", (unsigned int)encoded.data[j]);
            } 
            fputs("\n", stderr);
            failed = PR_TRUE;
        }
        PORT_Free(encoded.data);
    }

    if (failed) {
        fprintf(stderr, "FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
