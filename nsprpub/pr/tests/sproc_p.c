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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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

int main()
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
