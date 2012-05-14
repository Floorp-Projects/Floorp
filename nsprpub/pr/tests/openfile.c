/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

int main(int argc, char **argv)
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
