/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * File: pipeself.c
 *
 * Description:
 * This test creates a pipe, writes to it, and reads from it.
 */

#include "prio.h"
#include "prerror.h"

#include <stdio.h>
#include <string.h>

#define NUM_ITERATIONS 10

#define ENGLISH_GREETING "Hello"
#define SPANISH_GREETING "Hola"

int main()
{
    PRFileDesc *readPipe, *writePipe;
    char buf[128];
    int len;
    int idx;

    if (PR_CreatePipe(&readPipe, &writePipe) == PR_FAILURE) {
        fprintf(stderr, "cannot create pipe: (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        return 1;
    }

    for (idx = 0; idx < NUM_ITERATIONS; idx++) {
        strcpy(buf, ENGLISH_GREETING);
        len = strlen(buf) + 1;
        if (PR_Write(writePipe, buf, len) < len) {
            fprintf(stderr, "write failed\n");
            return 1;
        }
        memset(buf, 0, sizeof(buf));
        if (PR_Read(readPipe, buf, sizeof(buf)) == -1) {
            fprintf(stderr, "read failed: (%d, %d)\n",
                    PR_GetError(), PR_GetOSError());
            return 1;
        }
        if (strcmp(buf, ENGLISH_GREETING) != 0) {
            fprintf(stderr, "expected \"%s\" but got \"%s\"\n",
                    ENGLISH_GREETING, buf);
            return 1;
        }

        strcpy(buf, SPANISH_GREETING);
        len = strlen(buf) + 1;
        if (PR_Write(writePipe, buf, len) < len) {
            fprintf(stderr, "write failed\n");
            return 1;
        }
        memset(buf, 0, sizeof(buf));
        if (PR_Read(readPipe, buf, sizeof(buf)) == -1) {
            fprintf(stderr, "read failed: (%d, %d)\n",
                    PR_GetError(), PR_GetOSError());
            return 1;
        }
        if (strcmp(buf, SPANISH_GREETING) != 0) {
            fprintf(stderr, "expected \"%s\" but got \"%s\"\n",
                    SPANISH_GREETING, buf);
            return 1;
        }
    }

    if (PR_Close(readPipe) == PR_FAILURE) {
        fprintf(stderr, "close failed\n");
        return 1;
    }
    if (PR_Close(writePipe) == PR_FAILURE) {
        fprintf(stderr, "close failed\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
