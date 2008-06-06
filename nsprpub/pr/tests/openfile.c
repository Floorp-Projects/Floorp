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
