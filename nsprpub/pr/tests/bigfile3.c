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

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define TEST_FILE_NAME "bigfile3.txt"

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

#ifdef _WIN32
    hFile = CreateFile(TEST_FILE_NAME, GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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
    strcpy(buf, MESSAGE);
    if (WriteFile(hFile, buf, sizeof(buf), &nbytes, NULL) == 0) {
        fprintf(stderr, "WriteFile failed\n");
        exit(1);
    }
    PR_ASSERT(nbytes == sizeof(buf));
    if (CloseHandle(hFile) == 0) {
        fprintf(stderr, "CloseHandle failed\n");
        exit(1);
    }
#endif /* _WIN32 */

    memset(buf, 0, sizeof(buf));

    fd = PR_Open(TEST_FILE_NAME, PR_RDONLY, 0666);
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
    nbytes = PR_Read(fd, buf, sizeof(buf));
    if (nbytes != sizeof(buf)) {
        fprintf(stderr, "PR_Read failed\n");
        exit(1);
    }
    if (strcmp(buf, MESSAGE)) {
        fprintf(stderr, "corrupt data:$%s$\n", buf);
        exit(1);
    }
    if (PR_Close(fd) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }

    if (PR_Delete(TEST_FILE_NAME) == PR_FAILURE) {
        fprintf(stderr, "PR_Delete failed\n");
        exit(1);
    }

    printf("PASS\n");
    return 0;
}
