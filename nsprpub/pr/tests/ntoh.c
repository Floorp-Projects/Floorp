/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
