/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
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

/*
 * A test program for PR_htons, PR_ntohs, PR_htonl, PR_ntohl,
 * PR_htonll, and PR_ntohll.
 */

#include "prnetdb.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Byte sequence in network byte order */
static unsigned char bytes_n[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

/* Integers in host byte order */
static PRUint16 s_h = 0x0102;
static PRUint32 l_h = 0x01020304;
static PRUint64 ll_h = LL_INIT(0x01020304, 0x05060708);

int main()
{
    union {
        PRUint16 s;
        PRUint32 l;
        PRUint64 ll;
        unsigned char bytes[8];
    } un;

    un.s = s_h;
    printf("%u %u\n",
        un.bytes[0], un.bytes[1]);
    un.s = PR_htons(un.s);
    printf("%u %u\n",
        un.bytes[0], un.bytes[1]);
    if (memcmp(un.bytes, bytes_n, 2)) {
        fprintf(stderr, "PR_htons failed\n");
        exit(1);
    }
    un.s = PR_ntohs(un.s);
    printf("%u %u\n",
        un.bytes[0], un.bytes[1]);
    if (un.s != s_h) {
        fprintf(stderr, "PR_ntohs failed\n");
        exit(1);
    }

    un.l = l_h;
    printf("%u %u %u %u\n",
        un.bytes[0], un.bytes[1], un.bytes[2], un.bytes[3]);
    un.l = PR_htonl(un.l);
    printf("%u %u %u %u\n",
        un.bytes[0], un.bytes[1], un.bytes[2], un.bytes[3]);
    if (memcmp(un.bytes, bytes_n, 4)) {
        fprintf(stderr, "PR_htonl failed\n");
        exit(1);
    }
    un.l = PR_ntohl(un.l);
    printf("%u %u %u %u\n",
        un.bytes[0], un.bytes[1], un.bytes[2], un.bytes[3]);
    if (un.l != l_h) {
        fprintf(stderr, "PR_ntohl failed\n");
        exit(1);
    }

    un.ll = ll_h;
    printf("%u %u %u %u %u %u %u %u\n",
        un.bytes[0], un.bytes[1], un.bytes[2], un.bytes[3],
        un.bytes[4], un.bytes[5], un.bytes[6], un.bytes[7]);
    un.ll = PR_htonll(un.ll);
    printf("%u %u %u %u %u %u %u %u\n",
        un.bytes[0], un.bytes[1], un.bytes[2], un.bytes[3],
        un.bytes[4], un.bytes[5], un.bytes[6], un.bytes[7]);
    if (memcmp(un.bytes, bytes_n, 8)) {
        fprintf(stderr, "PR_htonll failed\n");
        exit(1);
    }
    un.ll = PR_ntohll(un.ll);
    printf("%u %u %u %u %u %u %u %u\n",
        un.bytes[0], un.bytes[1], un.bytes[2], un.bytes[3],
        un.bytes[4], un.bytes[5], un.bytes[6], un.bytes[7]);
    if (LL_NE(un.ll, ll_h)) {
        fprintf(stderr, "PR_ntohll failed\n");
        exit(1);
    }

    printf("PASS\n");
    return 0;
}
