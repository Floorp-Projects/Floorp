/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

int
main()
{
    PRBool failed = PR_FALSE;
    unsigned int i;
    unsigned int j;

    for (i = 0; i < sizeof(testCase) / sizeof(testCase[0]); i++) {
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
