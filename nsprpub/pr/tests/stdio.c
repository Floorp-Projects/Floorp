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
