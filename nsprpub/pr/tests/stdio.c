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
 * File:        stdio.c
 * Description: testing the "special" fds
 * Modification History:
 * 20-May-1997 AGarcia - Replace Test succeeded status with PASS. This is used by the
 *						regress tool parsing code.
 ** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**			recognize the return code from tha main program.
 */


#include "prlog.h"
#include "prinit.h"
#include "prio.h"

#include <stdio.h>
#include <string.h>

static PRIntn PR_CALLBACK stdio(PRIntn argc, char **argv)
{
    PRInt32 rv;

    PRFileDesc *out = PR_GetSpecialFD(PR_StandardOutput);
    PRFileDesc *err = PR_GetSpecialFD(PR_StandardError);

    rv = PR_Write(
        out, "This to standard out\n",
        strlen("This to standard out\n"));
    PR_ASSERT((PRInt32)strlen("This to standard out\n") == rv);
    rv = PR_Write(
        err, "This to standard err\n",
        strlen("This to standard err\n"));
    PR_ASSERT((PRInt32)strlen("This to standard err\n") == rv);

    return 0;

}  /* stdio */

int main(int argc, char **argv)
{
    PR_STDIO_INIT();
    return PR_Initialize(stdio, argc, argv, 0);
}  /* main */


/* stdio.c */
