/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define TEST_FILE_NAME "bigfile2.txt"
#ifdef WINCE
#define TEST_FILE_NAME_FOR_CREATEFILE   L"bigfile2.txt"
#else
#define TEST_FILE_NAME_FOR_CREATEFILE   TEST_FILE_NAME
#endif

#define MESSAGE "Hello world!"
#define MESSAGE_SIZE 13

int main(int argc, char **argv)
{
    PRFileDesc *fd;
    PRInt64 offset, position;
    PRInt32 nbytes;
    char buf[MESSAGE_SIZE];
#ifdef _WIN32
    HANDLE hFile;
    LARGE_INTEGER li;
#endif /* _WIN32 */

    LL_I2L(offset, 1);
    LL_SHL(offset, offset, 32);

    fd = PR_Open(TEST_FILE_NAME,
                 PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0666);
    if (fd == NULL) {
        fprintf(stderr, "PR_Open failed\n");
        exit(1);
    }
    position = PR_Seek64(fd, offset, PR_SEEK_SET);
    if (!LL_GE_ZERO(position)) {
        fprintf(stderr, "PR_Seek64 failed\n");
        exit(1);
    }
    PR_ASSERT(LL_EQ(position, offset));
    strcpy(buf, MESSAGE);
    nbytes = PR_Write(fd, buf, sizeof(buf));
    if (nbytes != sizeof(buf)) {
        fprintf(stderr, "PR_Write failed\n");
        exit(1);
    }
    if (PR_Close(fd) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }

    memset(buf, 0, sizeof(buf));

#ifdef _WIN32
    hFile = CreateFile(TEST_FILE_NAME_FOR_CREATEFILE, GENERIC_READ, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "CreateFile failed\n");
        exit(1);
    }
    li.QuadPart = offset;
    li.LowPart = SetFilePointer(hFile, li.LowPart, &li.HighPart, FILE_BEGIN);
    if (li.LowPart == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
        fprintf(stderr, "SetFilePointer failed\n");
        exit(1);
    }
    PR_ASSERT(li.QuadPart == offset);
    if (ReadFile(hFile, buf, sizeof(buf), &nbytes, NULL) == 0) {
        fprintf(stderr, "ReadFile failed\n");
        exit(1);
    }
    PR_ASSERT(nbytes == sizeof(buf));
    if (strcmp(buf, MESSAGE)) {
        fprintf(stderr, "corrupt data:$%s$\n", buf);
        exit(1);
    }
    if (CloseHandle(hFile) == 0) {
        fprintf(stderr, "CloseHandle failed\n");
        exit(1);
    }
#endif /* _WIN32 */

    if (PR_Delete(TEST_FILE_NAME) == PR_FAILURE) {
        fprintf(stderr, "PR_Delete failed\n");
        exit(1);
    }

    printf("PASS\n");
    return 0;
}
