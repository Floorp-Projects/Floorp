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
