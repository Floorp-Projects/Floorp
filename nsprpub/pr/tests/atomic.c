/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"
#include "prprf.h"
#include "pratom.h"

/*
 * TODO: create a macro to generate the six lines of code that are repeated
 * for every test.  Also rewrite the statement
 *     result = result | ((EXPRESSION) ? 0 : 1);
 * as
 *     result |= !(EXPRESSION);
 */

int main(int argc, char **argv)
{
    PRInt32 rv, oldval, test, result = 0;
    PRFileDesc *output = PR_GetSpecialFD(PR_StandardOutput);

    /***********************/
    /* Test the functions. */
    /***********************/

    oldval = test = -2;
    rv = PR_AtomicIncrement(&test);
    result = result | ((rv == -1) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicIncrement(%d) == %d: %s\n",
        oldval, rv, (rv == -1) ? "PASSED" : "FAILED");
    oldval = test;
    rv = PR_AtomicIncrement(&test);
    result = result | ((rv == 0) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicIncrement(%d) == %d: %s\n",
        oldval, rv, (rv == 0) ? "PASSED" : "FAILED");
    oldval = test;
    rv = PR_AtomicIncrement(&test);
    result = result | ((rv == 1) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicIncrement(%d) == %d: %s\n",
        oldval, rv, (rv == 1) ? "PASSED" : "FAILED");

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

    oldval = test = 2;
    rv = PR_AtomicDecrement(&test);
    result = result | ((rv == 1) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicDecrement(%d) == %d: %s\n",
        oldval, rv, (rv == 1) ? "PASSED" : "FAILED");
    oldval = test;
    rv = PR_AtomicDecrement(&test);
    result = result | ((rv == 0) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicDecrement(%d) == %d: %s\n",
        oldval, rv, (rv == 0) ? "PASSED" : "FAILED");
    oldval = test;
    rv = PR_AtomicDecrement(&test);
    result = result | ((rv == -1) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicDecrement(%d) == %d: %s\n",
        oldval, rv, (rv == -1) ? "PASSED" : "FAILED");

    /* set to a different value */
    oldval = test = -2;
    rv = PR_AtomicSet(&test, 2);
    result = result | (((rv == -2) && (test == 2)) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicSet(%d, %d) == %d: %s\n",
        oldval, 2, rv, ((rv == -2) && (test == 2)) ? "PASSED" : "FAILED");

    /* set to the same value */
    oldval = test = -2;
    rv = PR_AtomicSet(&test, -2);
    result = result | (((rv == -2) && (test == -2)) ? 0 : 1);
    PR_fprintf(
        output, "PR_AtomicSet(%d, %d) == %d: %s\n",
        oldval, -2, rv, ((rv == -2) && (test == -2)) ? "PASSED" : "FAILED");

    /***********************/
    /* Test the macros.    */
    /***********************/

    oldval = test = -2;
    rv = PR_ATOMIC_INCREMENT(&test);
    result = result | ((rv == -1) ? 0 : 1);
    PR_fprintf(
        output, "PR_ATOMIC_INCREMENT(%d) == %d: %s\n",
        oldval, rv, (rv == -1) ? "PASSED" : "FAILED");
    oldval = test;
    rv = PR_ATOMIC_INCREMENT(&test);
    result = result | ((rv == 0) ? 0 : 1);
    PR_fprintf(
        output, "PR_ATOMIC_INCREMENT(%d) == %d: %s\n",
        oldval, rv, (rv == 0) ? "PASSED" : "FAILED");
    oldval = test;
    rv = PR_ATOMIC_INCREMENT(&test);
    result = result | ((rv == 1) ? 0 : 1);
    PR_fprintf(
        output, "PR_ATOMIC_INCREMENT(%d) == %d: %s\n",
        oldval, rv, (rv == 1) ? "PASSED" : "FAILED");

    oldval = test = -2;
    rv = PR_ATOMIC_ADD(&test,1);
    result = result | ((rv == -1) ? 0 : 1);
    PR_fprintf(
        output, "PR_ATOMIC_ADD(%d,%d) == %d: %s\n",
        oldval, 1, rv, (rv == -1) ? "PASSED" : "FAILED");
    oldval = test;
    rv = PR_ATOMIC_ADD(&test, 4);
    result = result | ((rv == 3) ? 0 : 1);
    PR_fprintf(
        output, "PR_ATOMIC_ADD(%d,%d) == %d: %s\n",
        oldval, 4, rv, (rv == 3) ? "PASSED" : "FAILED");
    oldval = test;
    rv = PR_ATOMIC_ADD(&test, -6);
    result = result | ((rv == -3) ? 0 : 1);
    PR_fprintf(
        output, "PR_ATOMIC_ADD(%d,%d) == %d: %s\n",
        oldval, -6, rv, (rv == -3) ? "PASSED" : "FAILED");

    oldval = test = 2;
    rv = PR_ATOMIC_DECREMENT(&test);
    result = result | ((rv == 1) ? 0 : 1);
    PR_fprintf(
        output, "PR_ATOMIC_DECREMENT(%d) == %d: %s\n",
        oldval, rv, (rv == 1) ? "PASSED" : "FAILED");
    oldval = test;
    rv = PR_ATOMIC_DECREMENT(&test);
    result = result | ((rv == 0) ? 0 : 1);
    PR_fprintf(
        output, "PR_ATOMIC_DECREMENT(%d) == %d: %s\n",
        oldval, rv, (rv == 0) ? "PASSED" : "FAILED");
    oldval = test;
    rv = PR_ATOMIC_DECREMENT(&test);
    result = result | ((rv == -1) ? 0 : 1);
    PR_fprintf(
        output, "PR_ATOMIC_DECREMENT(%d) == %d: %s\n",
        oldval, rv, (rv == -1) ? "PASSED" : "FAILED");

    /* set to a different value */
    oldval = test = -2;
    rv = PR_ATOMIC_SET(&test, 2);
    result = result | (((rv == -2) && (test == 2)) ? 0 : 1);
    PR_fprintf(
        output, "PR_ATOMIC_SET(%d, %d) == %d: %s\n",
        oldval, 2, rv, ((rv == -2) && (test == 2)) ? "PASSED" : "FAILED");

    /* set to the same value */
    oldval = test = -2;
    rv = PR_ATOMIC_SET(&test, -2);
    result = result | (((rv == -2) && (test == -2)) ? 0 : 1);
    PR_fprintf(
        output, "PR_ATOMIC_SET(%d, %d) == %d: %s\n",
        oldval, -2, rv, ((rv == -2) && (test == -2)) ? "PASSED" : "FAILED");

    PR_fprintf(
        output, "Atomic operations test %s\n",
        (result == 0) ? "PASSED" : "FAILED");
    return result;
}  /* main */

/* atomic.c */
