/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:        rmdir.c
** Description: Demonstrate bugzilla 80884.
**
** after fix to unix_errors.c, message should report correct
** failure of PR_Rmdir().
**
**
**
*/

#include <prio.h>
#include <stdio.h>
#include <prerror.h>
#include <prlog.h>
#include "plgetopt.h"

#define DIRNAME "xxxBug80884/"
#define FILENAME "file80883"

PRBool  failed_already = PR_FALSE;
PRBool  debug_mode = PR_FALSE;

PRLogModuleInfo     *lm;

/*
** Help() -- print Usage information
*/
static void Help( void )  {
    fprintf(stderr, "template usage:\n"
            "\t-d     debug mode\n"
           );
} /* --- end Help() */

int main(int argc, char **argv)
{
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dh");
    PRFileDesc* fd;
    PRErrorCode err;

    /* parse command line options */
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
        if (PL_OPT_BAD == os) {
            continue;
        }
        switch (opt->option) {
            case 'd':  /* debug mode */
                debug_mode = PR_TRUE;
                break;
            case 'h':
            default:
                Help();
                return 2;
        }
    }
    PL_DestroyOptState(opt);

    lm = PR_NewLogModule( "testcase" );

    (void) PR_MkDir( DIRNAME, 0777);
    fd = PR_Open( DIRNAME FILENAME, PR_CREATE_FILE|PR_RDWR, 0666);
    if (fd == 0) {
        PRErrorCode err = PR_GetError();
        fprintf(stderr, "create file fails: %d: %s\n", err,
                PR_ErrorToString(err, PR_LANGUAGE_I_DEFAULT));
        failed_already = PR_TRUE;
        goto Finished;
    }

    PR_Close(fd);

    if (PR_RmDir( DIRNAME ) == PR_SUCCESS) {
        fprintf(stderr, "remove directory succeeds\n");
        failed_already = PR_TRUE;
        goto Finished;
    }

    err = PR_GetError();
    fprintf(stderr, "remove directory fails with: %d: %s\n", err,
            PR_ErrorToString(err, PR_LANGUAGE_I_DEFAULT));

    (void) PR_Delete( DIRNAME FILENAME);
    (void) PR_RmDir( DIRNAME );

    return 0;

Finished:
    if ( debug_mode ) {
        printf("%s\n", ( failed_already ) ? "FAILED" : "PASS" );
    }
    return( (failed_already)? 1 : 0 );
}  /* --- end main() */
/* --- end template.c */
