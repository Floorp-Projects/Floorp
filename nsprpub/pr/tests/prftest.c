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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * File:	prftest.c
 * Description:
 *     This is a simple test of the PR_snprintf() function defined
 *     in prprf.c.
 */

#include "prlong.h"
#include "prprf.h"

#include <string.h>

#define BUF_SIZE 128

int main() {
    PRInt16 i16;
    PRIntn n;
    PRInt32 i32;
    PRInt64 i64;
    char buf[BUF_SIZE];
    char answer[BUF_SIZE];
    int i, rv = 0;

    i16 = -1;
    n = -1;
    i32 = -1;
    LL_I2L(i64, i32);

    PR_snprintf(buf, BUF_SIZE, "%hx %x %lx %llx", i16, n, i32, i64);
    strcpy(answer, "ffff ");
    for (i = PR_BYTES_PER_INT * 2; i; i--) {
		strcat(answer, "f");
    }
    strcat(answer, " ffffffff ffffffffffffffff");

    if (!strcmp(buf, answer)) {
		printf("PR_snprintf test 1 passed\n");
    } else {
		printf("PR_snprintf test 1 failed\n");
		printf("Converted string is %s\n", buf);
		printf("Should be %s\n", answer);
		rv = 1;
    }

    i16 = -32;
    n = 30;
    i32 = 64;
    LL_I2L(i64, 333);
    PR_snprintf(buf, BUF_SIZE, "%d %hd %lld %ld", n, i16, i64, i32);
    if (!strcmp(buf, "30 -32 333 64")) {
		printf("PR_snprintf test 2 passed\n");
    } else {
		printf("PR_snprintf test 2 failed\n");
		printf("Converted string is %s\n", buf);
		printf("Should be 30 -32 333 64\n");
		rv = 1;
    }

    return rv;
}
