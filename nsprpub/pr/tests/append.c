/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
** File:        append.c
** Description: Testing File writes where PR_APPEND was used on open
**
** append attempts to verify that a file opened with PR_APPEND
** will always append to the end of file, regardless where the
** current file pointer is positioned. To do this, PR_Seek() is
** called before each write with the position set to beginning of
** file. Subsequent writes should always append.
** The file is read back, summing the integer data written to the
** file. If the expected result is equal, the test passes.
**
** See BugSplat: 4090
*/
#include "plgetopt.h"
#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>

PRIntn  debug = 0;
PRIntn  verbose = 0;
PRBool  failedAlready = PR_FALSE;
const PRInt32 addedBytes = 1000;
const PRInt32   buf = 1; /* constant written to fd, addedBytes times */
PRInt32         inBuf;   /* read it back into here */

PRIntn main(PRIntn argc, char *argv[])
{
    PRStatus    rc;
    PRInt32     rv;
    PRFileDesc  *fd;
    PRIntn      i;
    PRInt32     sum = 0;

    {   /* Get command line options */
        PLOptStatus os;
        PLOptState *opt = PL_CreateOptState(argc, argv, "vd");

	    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
        {
		    if (PL_OPT_BAD == os) continue;
            switch (opt->option)
            {
            case 'd':  /* debug */
                debug = 1;
                break;
            case 'v':  /* verbose */
                verbose = 1;
                break;
             default:
                break;
            }
        }
	    PL_DestroyOptState(opt);
    } /* end block "Get command line options" */
/* ---------------------------------------------------------------------- */
    fd = PR_Open( "/tmp/nsprAppend", (PR_APPEND | PR_CREATE_FILE | PR_TRUNCATE | PR_WRONLY), 0666 );
    if ( NULL == fd )  {
        if (debug) printf("PR_Open() failed for writing: %d\n", PR_GetError());
        failedAlready = PR_TRUE;
        goto Finished;
    }

    for ( i = 0; i < addedBytes ; i++ ) {
        rv = PR_Write( fd, &buf, sizeof(buf));
        if ( sizeof(buf) != rv )  {
            if (debug) printf("PR_Write() failed: %d\n", PR_GetError());
            failedAlready = PR_TRUE;
            goto Finished;
        }
        rv = PR_Seek( fd, 0 , PR_SEEK_SET );
        if ( -1 == rv )  {
            if (debug) printf("PR_Seek() failed: %d\n", PR_GetError());
            failedAlready = PR_TRUE;
            goto Finished;
        }
    }
    rc = PR_Close( fd );
    if ( PR_FAILURE == rc ) {
        if (debug) printf("PR_Close() failed after writing: %d\n", PR_GetError());
        failedAlready = PR_TRUE;
        goto Finished;
    }
/* ---------------------------------------------------------------------- */
    fd = PR_Open( "/tmp/nsprAppend", PR_RDONLY, 0 );
    if ( NULL == fd )  {
        if (debug) printf("PR_Open() failed for reading: %d\n", PR_GetError());
        failedAlready = PR_TRUE;
        goto Finished;
    }

    for ( i = 0; i < addedBytes ; i++ ) {
        rv = PR_Read( fd, &inBuf, sizeof(inBuf));
        if ( sizeof(inBuf) != rv)  {
            if (debug) printf("PR_Write() failed: %d\n", PR_GetError());
            failedAlready = PR_TRUE;
            goto Finished;
        }
        sum += inBuf;
    }

    rc = PR_Close( fd );
    if ( PR_FAILURE == rc ) {
        if (debug) printf("PR_Close() failed after reading: %d\n", PR_GetError());
        failedAlready = PR_TRUE;
        goto Finished;
    }
    if ( sum != addedBytes )  {
        if (debug) printf("Uh Oh! addedBytes: %d. Sum: %d\n", addedBytes, sum);
        failedAlready = PR_TRUE;
        goto Finished;
    }

/* ---------------------------------------------------------------------- */
Finished:
    if (debug || verbose) printf("%s\n", (failedAlready)? "FAILED" : "PASSED" );
    return( (failedAlready)? 1 : 0 );
}  /* main() */

/* append.c */
