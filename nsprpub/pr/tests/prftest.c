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
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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
