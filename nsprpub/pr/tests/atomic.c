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
 
#include "prio.h"
#include "prprf.h"
#include "pratom.h"

PRIntn main(PRIntn argc, char **argv)
{
    PRInt32 rv, oldval, test, result = 0;
    PRFileDesc *output = PR_GetSpecialFD(PR_StandardOutput);

    test = -2;
    rv = PR_AtomicIncrement(&test);
    result = result | ((rv < 0) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicIncrement(%d) == %d: %s\n",
        test, rv, (rv < 0) ? "PASSED" : "FAILED");
    rv = PR_AtomicIncrement(&test);
    result = result | ((rv == 0) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicIncrement(%d) == %d: %s\n",
        test, rv, (rv == 0) ? "PASSED" : "FAILED");
    rv = PR_AtomicIncrement(&test);
    result = result | ((rv > 0) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicIncrement(%d) == %d: %s\n",
        test, rv, (rv > 0) ? "PASSED" : "FAILED");

    oldval = test = -2;
    rv = PR_AtomicAdd(&test,1);
    result = result | ((rv == -1) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicAdd(%d,%d) == %d: %s\n",
        oldval, 1, rv, (rv == -1) ? "PASSED" : "FAILED");
	oldval = test;
    rv = PR_AtomicAdd(&test, 4);
    result = result | ((rv == 3) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicAdd(%d,%d) == %d: %s\n",
        oldval, 4, rv, (rv == 3) ? "PASSED" : "FAILED");
	oldval = test;
    rv = PR_AtomicAdd(&test, -6);
    result = result | ((rv == -3) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicAdd(%d,%d) == %d: %s\n",
        oldval, -6, rv, (rv == -3) ? "PASSED" : "FAILED");

    test = 2;
    rv = PR_AtomicDecrement(&test);
    result = result | ((rv > 0) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicDecrement(%d) == %d: %s\n",
        test, rv, (rv > 0) ? "PASSED" : "FAILED");
    rv = PR_AtomicDecrement(&test);
    result = result | ((rv == 0) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicDecrement(%d) == %d: %s\n",
        test, rv, (rv == 0) ? "PASSED" : "FAILED");
    rv = PR_AtomicDecrement(&test);
    result = result | ((rv < 0) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicDecrement(%d) == %d: %s\n",
        test, rv, (rv < 0) ? "PASSED" : "FAILED");

    /* set to a different value */
    test = -2;
    rv = PR_AtomicSet(&test, 2);
    result = result | (((rv == -2) && (test == 2)) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicSet(%d) == %d: %s\n",
        test, rv, ((rv == -2) && (test == 2)) ? "PASSED" : "FAILED");

    /* set to the same value */
    test = -2;
    rv = PR_AtomicSet(&test, -2);
    result = result | (((rv == -2) && (test == -2)) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicSet(%d) == %d: %s\n",
        test, rv, ((rv == -2) && (test == -2)) ? "PASSED" : "FAILED");

    PR_fprintf(
        output, "Atomic operations test %s\n",
        (result == 0) ? "PASSED" : "FAILED");
    return result;
}  /* main */

/* atomic.c */
