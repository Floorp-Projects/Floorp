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
** File:        env.c
** Description: Testing environment variable operations
**
*/
#include "prenv.h"
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>

PRIntn  debug = 0;
PRIntn  verbose = 0;
PRBool  failedAlready = PR_FALSE;

#define  ENVNAME    "NSPR_ENVIRONMENT_TEST_VARIABLE"
#define  ENVVALUE   "The expected result"
#define  ENVBUFSIZE 256

char    *envBuf; /* buffer pointer. We leak memory here on purpose! */

static char * NewBuffer( size_t size )
{
    char *buf = malloc( size );
    if ( NULL == buf ) {
        printf("env: NewBuffer() failed\n");
        exit(1);
    }
    return(buf);
} /* end NewBuffer() */

PRIntn main(PRIntn argc, char *argv[])
{
    char    *value;
    PRStatus    rc;

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

#if 0 
    {
        /*
        ** This uses Windows native environment manipulation
        ** as an experiment. Note the separation of namespace!
        */
        BOOL rv;
        DWORD   size;
        rv = SetEnvironmentVariable( ENVNAME, ENVVALUE );
        if ( rv == 0 )  {
            if (debug) printf("env: Shit! SetEnvironmentVariable() failed\n");
            failedAlready = PR_TRUE;
        }
        if (verbose) printf("env: SetEnvironmentVariable() worked\n");

        size = GetEnvironmentVariable( ENVNAME, envBuf, ENVBUFSIZE );    
        if ( size == 0 )  {
            if (debug) printf("env: Shit! GetEnvironmentVariable() failed. Found: %s\n", envBuf );
            failedAlready = PR_TRUE;
        }
        if (verbose) printf("env: GetEnvironmentVariable() worked. Found: %s\n", envBuf);

        value = PR_GetEnv( ENVNAME );
        if ( (NULL == value ) || (strcmp( value, ENVVALUE)))  {
            if (debug) printf( "env: PR_GetEnv() failed retrieving WinNative. Found: %s\n", value);
            failedAlready = PR_TRUE;
        }
        if (verbose) printf("env: PR_GetEnv() worked. Found: %s\n", value);
    }
#endif

    /* set an environment variable, read it back */
    envBuf = NewBuffer( ENVBUFSIZE );
    sprintf( envBuf, ENVNAME "=" ENVVALUE );
    rc = PR_SetEnv( envBuf );
    if ( PR_FAILURE == rc )  {
        if (debug) printf( "env: PR_SetEnv() failed setting\n");
        failedAlready = PR_TRUE;
    } else {
        if (verbose) printf("env: PR_SetEnv() worked.\n");
    }

    value = PR_GetEnv( ENVNAME );
    if ( (NULL == value ) || (strcmp( value, ENVVALUE)))  {
        if (debug) printf( "env: PR_GetEnv() Failed after setting\n" );
        failedAlready = PR_TRUE;
    } else {
        if (verbose) printf("env: PR_GetEnv() worked after setting it. Found: %s\n", value );
    }

/* ---------------------------------------------------------------------- */
    /* un-set the variable, using RAW name... should not work */
    envBuf = NewBuffer( ENVBUFSIZE );
    sprintf( envBuf, ENVNAME );
    rc = PR_SetEnv( envBuf );
    if ( PR_FAILURE == rc )  {
        if (verbose) printf( "env: PR_SetEnv() not un-set using RAW name. Good!\n");
    } else {
        if (debug) printf("env: PR_SetEnv() un-set using RAW name. Bad!\n" );
        failedAlready = PR_TRUE;
    }

    value = PR_GetEnv( ENVNAME );
    if ( NULL == value ) {
        if (debug) printf("env: PR_GetEnv() after un-set using RAW name. Bad!\n" );
        failedAlready = PR_TRUE;
    } else {
        if (verbose) printf( "env: PR_GetEnv() after RAW un-set found: %s\n", value );
    }
    
/* ---------------------------------------------------------------------- */
    /* set it again ... */
    envBuf = NewBuffer( ENVBUFSIZE );
    sprintf( envBuf, ENVNAME "=" ENVVALUE );
    rc = PR_SetEnv( envBuf );
    if ( PR_FAILURE == rc )  {
        if (debug) printf( "env: PR_SetEnv() failed setting the second time.\n");
        failedAlready = PR_TRUE;
    } else {
        if (verbose) printf("env: PR_SetEnv() worked.\n");
    }

    /* un-set the variable using the form name= */
    envBuf = NewBuffer( ENVBUFSIZE );
    sprintf( envBuf, ENVNAME "=" );
    rc = PR_SetEnv( envBuf );
    if ( PR_FAILURE == rc )  {
        if (debug) printf( "env: PR_SetEnv() failed un-setting using name=\n");
        failedAlready = PR_TRUE;
    } else {
        if (verbose) printf("env: PR_SetEnv() un-set using name= worked\n" );
    }

    value = PR_GetEnv( ENVNAME );
    if (( NULL == value ) || ( 0x00 == *value )) {
        if (verbose) printf("env: PR_GetEnv() after un-set using name= worked\n" );
    } else {
        if (debug) printf( "env: PR_GetEnv() after un-set using name=. Found: %s\n", value );
        failedAlready = PR_TRUE;
    }
/* ---------------------------------------------------------------------- */
    /* un-set the variable using the form name= */
    envBuf = NewBuffer( ENVBUFSIZE );
    sprintf( envBuf, ENVNAME "999=" );
    rc = PR_SetEnv( envBuf );
    if ( PR_FAILURE == rc )  {
        if (debug) printf( "env: PR_SetEnv() failed un-setting using name=\n");
        failedAlready = PR_TRUE;
    } else {
        if (verbose) printf("env: PR_SetEnv() un-set using name= worked\n" );
    }

    value = PR_GetEnv( ENVNAME "999" );
    if (( NULL == value ) || ( 0x00 == *value )) {
        if (verbose) printf("env: PR_GetEnv() after un-set using name= worked\n" );
    } else {
        if (debug) printf( "env: PR_GetEnv() after un-set using name=. Found: %s\n", value );
        failedAlready = PR_TRUE;
    }

/* ---------------------------------------------------------------------- */
    if (debug || verbose) printf("\n%s\n", (failedAlready)? "FAILED" : "PASSED" );
    return( (failedAlready)? 1 : 0 );
}  /* main() */

/* env.c */
