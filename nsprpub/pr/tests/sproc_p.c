/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test sproc_p.c
 *
 * The purpose of this test and the sproc_ch.c test is to test the shutdown
 * of all the IRIX sprocs in a program when one of them dies due to an error.
 *
 * In this test, the parent sproc gets a segmentation fault and dies.
 * The child sproc never stops on its own.  You should use "ps" to see if
 * the child sproc is killed after the parent dies.
 */

#include "prinit.h"
#include <stdio.h>

#if !defined(IRIX)

int main(int argc, char **argv)
{
    printf("This test applies to IRIX only.\n");
    return 0;
}

#else  /* IRIX */

#include "prthread.h"
#include <sys/types.h>
#include <unistd.h>

void NeverStops(void *unused)
{
    int i = 0;

    printf("The child sproc has pid %d.\n", getpid());
    printf("The child sproc won't stop on its own.\n");
    fflush(stdout);

    /* I never stop */
    while (1) {
	i++;
    }
}

int main()
{
    int *p = 0;

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

    printf("The parent sproc has pid %d.\n", getpid());
    printf("The parent sproc will first create a child sproc.\n");
    printf("Then the parent sproc will get a segmentation fault and die.\n");
    printf("The child sproc should be killed after the parent sproc dies.\n");
    printf("Use 'ps' to make sure this is so.\n");
    fflush(stdout);

    PR_CreateThread(PR_USER_THREAD, NeverStops, NULL,
	    PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);

    /* Force a segmentation fault */
    *p = 0;
    return 0;
}

#endif /* IRIX */
