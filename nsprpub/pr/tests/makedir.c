/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * This test calls PR_MakeDir to create a bunch of directories
 * with various mode bits.
 */

#include "prio.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    if (PR_MakeDir("tdir0400", 0400) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    if (PR_MakeDir("tdir0200", 0200) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    if (PR_MakeDir("tdir0100", 0100) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    if (PR_MakeDir("tdir0500", 0500) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    if (PR_MakeDir("tdir0600", 0600) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    if (PR_MakeDir("tdir0300", 0300) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    if (PR_MakeDir("tdir0700", 0700) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    if (PR_MakeDir("tdir0640", 0640) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    if (PR_MakeDir("tdir0660", 0660) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    if (PR_MakeDir("tdir0644", 0644) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    if (PR_MakeDir("tdir0664", 0664) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    if (PR_MakeDir("tdir0666", 0666) == PR_FAILURE) {
        fprintf(stderr, "PR_MakeDir failed\n");
        exit(1);
    }
    return 0;
}
