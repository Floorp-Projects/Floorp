/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

int main(int argc, char **argv)
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
