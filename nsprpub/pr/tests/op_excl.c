/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Name: op_excl.c
**
** Description: Test Program to verify function of PR_EXCL open flag
**
** Modification History:
** 27-Oct-1999 lth. Initial version
***********************************************************************/

#include <plgetopt.h>
#include <nspr.h>
#include <stdio.h>
#include <stdlib.h>

/*
** Test harness infrastructure
*/
PRLogModuleInfo *lm;
PRLogModuleLevel msgLevel = PR_LOG_NONE;
PRIntn  debug = 0;
PRUint32  failed_already = 0;
/* end Test harness infrastructure */
/*
** Emit help text for this test
*/
static void Help( void )
{
    printf("op_excl: Help");
    printf("op_excl [-d]");
    printf("-d enables debug messages");
    exit(1);
} /* end Help() */



int main(int argc, char **argv)
{
    PRFileDesc  *fd;
    PRStatus    rv;
    PRInt32     written;
    char        outBuf[] = "op_excl.c test file";
#define OUT_SIZE sizeof(outBuf)
#define NEW_FILENAME "xxxExclNewFile"

    {
        /*
        ** Get command line options
        */
        PLOptStatus os;
        PLOptState *opt = PL_CreateOptState(argc, argv, "hd");

        while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
        {
            if (PL_OPT_BAD == os) {
                continue;
            }
            switch (opt->option)
            {
                case 'd':  /* debug */
                    debug = 1;
                    msgLevel = PR_LOG_ERROR;
                    break;
                case 'h':  /* help message */
                    Help();
                    break;
                default:
                    break;
            }
        }
        PL_DestroyOptState(opt);
    }

    lm = PR_NewLogModule("Test");       /* Initialize logging */

    /*
    ** First, open a file, PR_EXCL, we believe not to exist
    */
    fd = PR_Open( NEW_FILENAME, PR_CREATE_FILE | PR_EXCL | PR_WRONLY, 0666 );
    if ( NULL == fd )  {
        if (debug) {
            fprintf( stderr, "Open exclusive. Expected success, got failure\n");
        }
        failed_already = 1;
        goto Finished;
    }

    written = PR_Write( fd, outBuf, OUT_SIZE );
    if ( OUT_SIZE != written )  {
        if (debug) {
            fprintf( stderr, "Write after open exclusive failed\n");
        }
        failed_already = 1;
        goto Finished;
    }

    rv = PR_Close(fd);
    if ( PR_FAILURE == rv )  {
        if (debug) {
            fprintf( stderr, "Close after open exclusive failed\n");
        }
        failed_already = 1;
        goto Finished;
    }

    /*
    ** Second, open the same file, PR_EXCL, expect it to fail
    */
    fd = PR_Open( NEW_FILENAME, PR_CREATE_FILE | PR_EXCL | PR_WRONLY, 0666 );
    if ( NULL != fd )  {
        if (debug) {
            fprintf( stderr, "Open exclusive. Expected failure, got success\n");
        }
        failed_already = 1;
        PR_Close(fd);
    }

    rv = PR_Delete( NEW_FILENAME );
    if ( PR_FAILURE == rv ) {
        if (debug) {
            fprintf( stderr, "PR_Delete() failed\n");
        }
        failed_already = 1;
    }

Finished:
    if (debug) {
        printf("%s\n", (failed_already)? "FAIL" : "PASS");
    }
    return( (failed_already == PR_TRUE )? 1 : 0 );
}  /* main() */
/* end op_excl.c */

