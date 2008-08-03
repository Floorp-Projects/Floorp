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
PRBool	debug_mode = PR_FALSE;

PRLogModuleInfo     *lm;

/*
** Help() -- print Usage information
*/
static void Help( void )  {
    fprintf(stderr, "template usage:\n"
                    "\t-d     debug mode\n"
                    );
} /* --- end Help() */

PRIntn main(PRIntn argc, char *argv[])
{
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dh");
    PRFileDesc* fd;
    PRErrorCode err;

    /* parse command line options */
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
        if (PL_OPT_BAD == os) continue;
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
    if ( debug_mode ) printf("%s\n", ( failed_already ) ? "FAILED" : "PASS" );
    return( (failed_already)? 1 : 0 );
}  /* --- end main() */
/* --- end template.c */
