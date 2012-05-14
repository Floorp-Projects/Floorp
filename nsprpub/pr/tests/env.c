/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:        env.c
** Description: Testing environment variable operations
**
*/
#include "prenv.h"
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char **argv)
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
