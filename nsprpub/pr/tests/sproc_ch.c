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
 * Test sproc_ch.c
 *
 * The purpose of this test and the sproc_p.c test is to test the shutdown
 * of all the IRIX sprocs in a program when one of them dies due to an error.
 *
 * There are three sprocs in this test: the parent, the child, and the 
 * grandchild.  The parent and child sprocs never stop on their own.
 * The grandchild sproc gets a segmentation fault and dies.  You should
 * You should use "ps" to see if the parent and child sprocs are killed
 * after the grandchild dies.
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

void SegFault(void *unused)
{
    int *p = 0;

    printf("The grandchild sproc has pid %d.\n", getpid());
    printf("The grandchild sproc will get a segmentation fault and die.\n");
    printf("The parent and child sprocs should be killed after the "
            "grandchild sproc dies.\n");
    printf("Use 'ps' to make sure this is so.\n");
    fflush(stdout);
    /* Force a segmentation fault */
    *p = 0;
}

void NeverStops(void *unused)
{
    int i = 0;

    printf("The child sproc has pid %d.\n", getpid());
    printf("The child sproc won't stop on its own.\n");
    fflush(stdout);

    /* create the grandchild sproc */
    PR_CreateThread(PR_USER_THREAD, SegFault, NULL,
	    PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);

    while (1) {
	i++;
    }
}

int main()
{
    int i= 0;

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

    printf("The parent sproc has pid %d.\n", getpid());
    printf("The parent sproc won't stop on its own.\n");
    fflush(stdout);

    /* create the child sproc */
    PR_CreateThread(PR_USER_THREAD, NeverStops, NULL,
	    PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);

    while (1) {
	i++;
    }
    return 0;
}

#endif  /* IRIX */
