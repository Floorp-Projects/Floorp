/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This test calls PR_MakeDir to create a bunch of directories
 * with various mode bits.
 */

#include "prio.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
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
