/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File: mbcs.c
**
** Synopsis: mbcs {dirName}
**
** where dirName is the directory to be traversed. dirName is required.
**
** Description:
** mbcs.c tests use of multi-byte characters, as would be passed to
** NSPR funtions by internationalized applications.
**
** mbcs.c, when run on any single-byte platform, should run correctly.
** In truth, running the mbcs test on a single-byte platform is
** really meaningless. mbcs.c, nor any NSPR library or test is not
** intended for use with any wide character set, including Unicode.
** mbcs.c should not be included in runtests.ksh because it requires
** extensive user intervention to set-up and run.
**
** mbcs.c should be run on a platform using some form of multi-byte
** characters. The initial platform for this test is a Japanese
** language Windows NT 4.0 machine. ... Thank you Noriko Hoshi.
**
** To run mbcs.c, the tester should create a directory tree containing
** some files in the same directory from which the test is run; i.e.
** the current working directory. The directory and files should be
** named such that when represented in the local multi-byte character
** set, one or more characters of the name is longer than a single
** byte.
**
*/

#include <plgetopt.h>
#include <nspr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
** Test harness infrastructure
*/
PRLogModuleInfo *lm;
PRLogModuleLevel msgLevel = PR_LOG_NONE;
PRIntn  debug = 0;
PRUint32  failed_already = 0;
/* end Test harness infrastructure */

char *dirName =  NULL;  /* directory name to traverse */

/*
** Traverse directory
*/
static void TraverseDirectory( unsigned char *dir )
{
    PRDir *cwd;
    PRDirEntry *dirEntry;
    PRFileInfo info;
    PRStatus rc;
    PRInt32 err;
    PRFileDesc *fd;
    char    nextDir[256];
    char    file[256];

    printf("Directory: %s\n", dir );
    cwd = PR_OpenDir( dir );
    if ( NULL == cwd )  {
        printf("PR_OpenDir() failed on directory: %s, with error: %d, %d\n",
               dir, PR_GetError(), PR_GetOSError());
        exit(1);
    }
    while( NULL != (dirEntry = PR_ReadDir( cwd, PR_SKIP_BOTH | PR_SKIP_HIDDEN )))  {
        sprintf( file, "%s/%s", dir, dirEntry->name );
        rc = PR_GetFileInfo( file, &info );
        if ( PR_FAILURE == rc ) {
            printf("PR_GetFileInfo() failed on file: %s, with error: %d, %d\n",
                   dirEntry->name, PR_GetError(), PR_GetOSError());
            exit(1);
        }
        if ( PR_FILE_FILE == info.type )  {
            printf("File: %s \tsize: %ld\n", dirEntry->name, info.size );
            fd = PR_Open( file, PR_RDONLY, 0 );
            if ( NULL == fd )  {
                printf("PR_Open() failed. Error: %ld, OSError: %ld\n",
                       PR_GetError(), PR_GetOSError());
            }
            rc = PR_Close( fd );
            if ( PR_FAILURE == rc )  {
                printf("PR_Close() failed. Error: %ld, OSError: %ld\n",
                       PR_GetError(), PR_GetOSError());
            }
        } else if ( PR_FILE_DIRECTORY == info.type ) {
            sprintf( nextDir, "%s/%s", dir, dirEntry->name );
            TraverseDirectory(nextDir);
        } else {
            printf("type is not interesting for file: %s\n", dirEntry->name );
            /* keep going */
        }
    }
    /* assume end-of-file, actually could be error */

    rc = PR_CloseDir( cwd );
    if ( PR_FAILURE == rc ) {
        printf("PR_CloseDir() failed on directory: %s, with error: %d, %d\n",
               dir, PR_GetError(), PR_GetOSError());
    }

} /* end TraverseDirectory() */

int main(int argc, char **argv)
{
    {   /* get command line options */
        /*
        ** Get command line options
        */
        PLOptStatus os;
        PLOptState *opt = PL_CreateOptState(argc, argv, "dv");

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
                case 'v':  /* verbose mode */
                    msgLevel = PR_LOG_DEBUG;
                    break;
                default:
                    dirName = strdup(opt->value);
                    break;
            }
        }
        PL_DestroyOptState(opt);
    } /* end get command line options */

    lm = PR_NewLogModule("Test");       /* Initialize logging */


    if ( dirName == NULL )  {
        printf("you gotta specify a directory as an operand!\n");
        exit(1);
    }

    TraverseDirectory( dirName );

    if (debug) {
        printf("%s\n", (failed_already)? "FAIL" : "PASS");
    }
    return( (failed_already == PR_TRUE )? 1 : 0 );
}  /* main() */
/* end template.c */
