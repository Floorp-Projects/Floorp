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
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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
 
#include "prio.h"
#include "prprf.h"
#include "pratom.h"

PRIntn main(PRIntn argc, char **argv)
{
    PRInt32 rv, oldval, test, result = 0;
    PRFileDesc *output = PR_GetSpecialFD(PR_StandardOutput);

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

    PR_fprintf(
        output, "Atomic operations test %s\n",
        (result == 0) ? "PASSED" : "FAILED");
    return result;
}  /* main */

/* atomic.c */
