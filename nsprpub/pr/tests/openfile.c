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
 * This test calls PR_OpenFile to create a bunch of files
 * with various file modes.
 */

#include "prio.h"
#include "prerror.h"
#include "prinit.h"

#include <stdio.h>
#include <stdlib.h>

#define TEMPLATE_FILE_NAME "template.txt"

int main()
{
    FILE *template;
    char buf[32];
    PRInt32 nbytes;
    PRFileDesc *fd;

    
    /* Write in text mode.  Let stdio deal with line endings. */
    template = fopen(TEMPLATE_FILE_NAME, "w");
    fputs("line 1\nline 2\n", template);
    fclose(template);

    /* Read in binary mode */
    fd = PR_OpenFile(TEMPLATE_FILE_NAME, PR_RDONLY, 0666);
    nbytes = PR_Read(fd, buf, sizeof(buf));
    PR_Close(fd);
    PR_Delete(TEMPLATE_FILE_NAME);

    fd = PR_OpenFile("tfil0700.txt", PR_RDWR | PR_CREATE_FILE, 0700);
    if (NULL == fd) {
        fprintf(stderr, "PR_OpenFile failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    PR_Write(fd, buf, nbytes);
    PR_Close(fd);

    fd = PR_OpenFile("tfil0500.txt", PR_RDWR | PR_CREATE_FILE, 0500);
    if (NULL == fd) {
        fprintf(stderr, "PR_OpenFile failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    PR_Write(fd, buf, nbytes);
    PR_Close(fd);

    fd = PR_OpenFile("tfil0400.txt", PR_RDWR | PR_CREATE_FILE, 0400);
    if (NULL == fd) {
        fprintf(stderr, "PR_OpenFile failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    PR_Write(fd, buf, nbytes);
    PR_Close(fd);

    fd = PR_OpenFile("tfil0644.txt", PR_RDWR | PR_CREATE_FILE, 0644);
    if (NULL == fd) {
        fprintf(stderr, "PR_OpenFile failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    PR_Write(fd, buf, nbytes);
    PR_Close(fd);

    fd = PR_OpenFile("tfil0664.txt", PR_RDWR | PR_CREATE_FILE, 0664);
    if (NULL == fd) {
        fprintf(stderr, "PR_OpenFile failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    PR_Write(fd, buf, nbytes);
    PR_Close(fd);

    fd = PR_OpenFile("tfil0660.txt", PR_RDWR | PR_CREATE_FILE, 0660);
    if (NULL == fd) {
        fprintf(stderr, "PR_OpenFile failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    PR_Write(fd, buf, nbytes);
    PR_Close(fd);

    fd = PR_OpenFile("tfil0666.txt", PR_RDWR | PR_CREATE_FILE, 0666);
    if (NULL == fd) {
        fprintf(stderr, "PR_OpenFile failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    PR_Write(fd, buf, nbytes);
    PR_Close(fd);

    fd = PR_OpenFile("tfil0640.txt", PR_RDWR | PR_CREATE_FILE, 0640);
    if (NULL == fd) {
        fprintf(stderr, "PR_OpenFile failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    PR_Write(fd, buf, nbytes);
    PR_Close(fd);

    PR_Cleanup();
    return 0;
}
