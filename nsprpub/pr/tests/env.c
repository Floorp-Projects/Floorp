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
#include "prmem.h"
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PRIntn  debug = 0;
PRIntn  verbose = 0;
PRIntn  secure = 0;
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
        PLOptState *opt = PL_CreateOptState(argc, argv, "vds");

        while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
        {
            if (PL_OPT_BAD == os) {
                continue;
            }
            switch (opt->option)
            {
                case 'd':  /* debug */
                    debug = 1;
                    break;
                case 'v':  /* verbose */
                    verbose = 1;
                    break;
                case 's':  /* secure / set[ug]id */
                    /*
                    ** To test PR_GetEnvSecure, make this executable (or a
                    ** copy of it) setuid / setgid / otherwise inherently
                    ** privileged (e.g., file capabilities) and run it
                    ** with this flag.
                    */
                    secure = 1;
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
            if (debug) {
                printf("env: Shit! SetEnvironmentVariable() failed\n");
            }
            failedAlready = PR_TRUE;
        }
        if (verbose) {
            printf("env: SetEnvironmentVariable() worked\n");
        }

        size = GetEnvironmentVariable( ENVNAME, envBuf, ENVBUFSIZE );
        if ( size == 0 )  {
            if (debug) {
                printf("env: Shit! GetEnvironmentVariable() failed. Found: %s\n", envBuf );
            }
            failedAlready = PR_TRUE;
        }
        if (verbose) {
            printf("env: GetEnvironmentVariable() worked. Found: %s\n", envBuf);
        }

        value = PR_GetEnv( ENVNAME );
        if ( (NULL == value ) || (strcmp( value, ENVVALUE)))  {
            if (debug) {
                printf( "env: PR_GetEnv() failed retrieving WinNative. Found: %s\n", value);
            }
            failedAlready = PR_TRUE;
        }
        if (verbose) {
            printf("env: PR_GetEnv() worked. Found: %s\n", value);
        }
    }
#endif

    /* set an environment variable, read it back */
    envBuf = NewBuffer( ENVBUFSIZE );
    sprintf( envBuf, ENVNAME "=" ENVVALUE );
    rc = PR_SetEnv( envBuf );
    if ( PR_FAILURE == rc )  {
        if (debug) {
            printf( "env: PR_SetEnv() failed setting\n");
        }
        failedAlready = PR_TRUE;
    } else {
        if (verbose) {
            printf("env: PR_SetEnv() worked.\n");
        }
    }

    value = PR_GetEnv( ENVNAME );
    if ( (NULL == value ) || (strcmp( value, ENVVALUE)))  {
        if (debug) {
            printf( "env: PR_GetEnv() Failed after setting\n" );
        }
        failedAlready = PR_TRUE;
    } else {
        if (verbose) {
            printf("env: PR_GetEnv() worked after setting it. Found: %s\n", value );
        }
    }

    if ( secure ) {
        /*
        ** In this case we've been run with elevated privileges, so
        ** test that PR_GetEnvSecure *doesn't* find that env var.
        */
        value = PR_GetEnvSecure( ENVNAME );
        if ( NULL != value ) {
            if (debug) {
                printf( "env: PR_GetEnvSecure() failed; expected NULL, found \"%s\"\n", value );
            }
            failedAlready = PR_TRUE;
        } else {
            if (verbose) {
                printf("env: PR_GetEnvSecure() worked\n" );
            }
        }
    } else {
        /*
        ** In this case the program is being run normally, so do the
        ** same check for PR_GetEnvSecure as for PR_GetEnv.
        */
        value = PR_GetEnvSecure( ENVNAME );
        if ( (NULL == value ) || (strcmp( value, ENVVALUE)))  {
            if (debug) {
                printf( "env: PR_GetEnvSecure() Failed after setting\n" );
            }
            failedAlready = PR_TRUE;
        } else {
            if (verbose) {
                printf("env: PR_GetEnvSecure() worked after setting it. Found: %s\n", value );
            }
        }
    }

    /* ---------------------------------------------------------------------- */
    /* check that PR_DuplicateEnvironment() agrees with PR_GetEnv() */
    {
#if defined(XP_UNIX) && (!defined(DARWIN) || defined(HAVE_CRT_EXTERNS_H))
        static const PRBool expect_failure = PR_FALSE;
#else
        static const PRBool expect_failure = PR_TRUE;
#endif
        char **i, **dupenv = PR_DuplicateEnvironment();


        if ( NULL == dupenv ) {
            if (expect_failure) {
                if (verbose) printf("env: PR_DuplicateEnvironment failed, "
                                        "as expected on this platform.\n");
            } else {
                if (debug) {
                    printf("env: PR_DuplicateEnvironment() failed.\n");
                }
                failedAlready = PR_TRUE;
            }
        } else {
            unsigned found = 0;

            if (expect_failure) {
                if (debug) printf("env: PR_DuplicateEnvironment() succeeded, "
                                      "but failure is expected on this platform.\n");
                failedAlready = PR_TRUE;
            } else {
                if (verbose) {
                    printf("env: PR_DuplicateEnvironment() succeeded.\n");
                }
            }
            for (i = dupenv; *i; i++) {
                char *equals = strchr(*i, '=');

                if ( equals == NULL ) {
                    if (debug) printf("env: PR_DuplicateEnvironment() returned a string"
                                          " with no '=': %s\n", *i);
                    failedAlready = PR_TRUE;
                } else {
                    /* We own this string, so we can temporarily alter it */
                    /* *i is the null-terminated name; equals + 1 is the value */
                    *equals = '\0';

                    if ( strcmp(*i, ENVNAME) == 0) {
                        found++;
                        if (verbose) printf("env: PR_DuplicateEnvironment() found " ENVNAME
                                                " (%u so far).\n", found);
                    }

                    /* Multiple values for the same name can't happen, according to POSIX. */
                    value = PR_GetEnv(*i);
                    if ( value == NULL ) {
                        if (debug) printf("env: PR_DuplicateEnvironment() returned a name"
                                              " which PR_GetEnv() failed to find: %s\n", *i);
                        failedAlready = PR_TRUE;
                    } else if ( strcmp(equals + 1, value) != 0) {
                        if (debug) printf("env: PR_DuplicateEnvironment() returned the wrong"
                                              " value for %s: expected %s; found %s\n",
                                              *i, value, equals + 1);
                        failedAlready = PR_TRUE;
                    } else {
                        if (verbose) printf("env: PR_DuplicateEnvironment() agreed with"
                                                " PR_GetEnv() about %s\n", *i);
                    }
                }
                PR_Free(*i);
            }
            PR_Free(dupenv);

            if (found != 1) {
                if (debug) printf("env: PR_DuplicateEnvironment() found %u entries for " ENVNAME
                                      " (expected 1)\n", found);
                failedAlready = PR_TRUE;
            } else {
                if (verbose) {
                    printf("env: PR_DuplicateEnvironment() found 1 entry for " ENVNAME "\n");
                }
            }
        }
    }

    /* ---------------------------------------------------------------------- */
    /* un-set the variable, using RAW name... should not work */
    envBuf = NewBuffer( ENVBUFSIZE );
    sprintf( envBuf, ENVNAME );
    rc = PR_SetEnv( envBuf );
    if ( PR_FAILURE == rc )  {
        if (verbose) {
            printf( "env: PR_SetEnv() not un-set using RAW name. Good!\n");
        }
    } else {
        if (debug) {
            printf("env: PR_SetEnv() un-set using RAW name. Bad!\n" );
        }
        failedAlready = PR_TRUE;
    }

    value = PR_GetEnv( ENVNAME );
    if ( NULL == value ) {
        if (debug) {
            printf("env: PR_GetEnv() after un-set using RAW name. Bad!\n" );
        }
        failedAlready = PR_TRUE;
    } else {
        if (verbose) {
            printf( "env: PR_GetEnv() after RAW un-set found: %s\n", value );
        }
    }

    /* ---------------------------------------------------------------------- */
    /* set it again ... */
    envBuf = NewBuffer( ENVBUFSIZE );
    sprintf( envBuf, ENVNAME "=" ENVVALUE );
    rc = PR_SetEnv( envBuf );
    if ( PR_FAILURE == rc )  {
        if (debug) {
            printf( "env: PR_SetEnv() failed setting the second time.\n");
        }
        failedAlready = PR_TRUE;
    } else {
        if (verbose) {
            printf("env: PR_SetEnv() worked.\n");
        }
    }

    /* un-set the variable using the form name= */
    envBuf = NewBuffer( ENVBUFSIZE );
    sprintf( envBuf, ENVNAME "=" );
    rc = PR_SetEnv( envBuf );
    if ( PR_FAILURE == rc )  {
        if (debug) {
            printf( "env: PR_SetEnv() failed un-setting using name=\n");
        }
        failedAlready = PR_TRUE;
    } else {
        if (verbose) {
            printf("env: PR_SetEnv() un-set using name= worked\n" );
        }
    }

    value = PR_GetEnv( ENVNAME );
    if (( NULL == value ) || ( 0x00 == *value )) {
        if (verbose) {
            printf("env: PR_GetEnv() after un-set using name= worked\n" );
        }
    } else {
        if (debug) {
            printf( "env: PR_GetEnv() after un-set using name=. Found: %s\n", value );
        }
        failedAlready = PR_TRUE;
    }
    /* ---------------------------------------------------------------------- */
    /* un-set the variable using the form name= */
    envBuf = NewBuffer( ENVBUFSIZE );
    sprintf( envBuf, ENVNAME "999=" );
    rc = PR_SetEnv( envBuf );
    if ( PR_FAILURE == rc )  {
        if (debug) {
            printf( "env: PR_SetEnv() failed un-setting using name=\n");
        }
        failedAlready = PR_TRUE;
    } else {
        if (verbose) {
            printf("env: PR_SetEnv() un-set using name= worked\n" );
        }
    }

    value = PR_GetEnv( ENVNAME "999" );
    if (( NULL == value ) || ( 0x00 == *value )) {
        if (verbose) {
            printf("env: PR_GetEnv() after un-set using name= worked\n" );
        }
    } else {
        if (debug) {
            printf( "env: PR_GetEnv() after un-set using name=. Found: %s\n", value );
        }
        failedAlready = PR_TRUE;
    }

    /* ---------------------------------------------------------------------- */
    if (debug || verbose) {
        printf("\n%s\n", (failedAlready)? "FAILED" : "PASSED" );
    }
    return( (failedAlready)? 1 : 0 );
}  /* main() */

/* env.c */
