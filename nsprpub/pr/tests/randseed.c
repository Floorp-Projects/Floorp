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
** File: rngseed.c
** Description: 
** Test NSPR's Random Number Seed generator
**
** Initial test: Just make sure it outputs some data.
** 
** ... more? ... check some iterations to ensure it is random (no dupes)
** ... more? ... histogram distribution of random numbers
*/

#include <plgetopt.h> 
#include <nspr.h> 
#include <prrng.h> 
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

PRIntn  optRandCount = 30;
char    buf[40];
PRSize  bufSize = sizeof(buf);
PRSize  rSize;
PRIntn  i;

/*
** Emit help text for this test
*/
static void Help( void )
{
    printf("Template: Help(): display your help message(s) here");
    exit(1);
} /* end Help() */

static void PrintRand( void *buf, PRIntn size )
{
    PRUint32 *rp = buf;
    PRIntn   i;

    printf("%4.4d--\n", size );
    while (size > 0 ) {
        switch( size )  {
            case 1 :
                printf("%2.2X\n", *(rp++) );
                size -= 4;    
                break;
            case 2 :
                printf("%4.4X\n", *(rp++) );
                size -= 4;    
                break;
            case 3 :
                printf("%6.6X\n", *(rp++) );
                size -= 4;    
                break;
            default:
                while ( size >= 4) {
                    PRIntn i = 3;
                    do {
                        printf("%8.8X ", *(rp++) );
                        size -= 4;    
                    } while( i-- );
                    i = 3;
                    printf("\n");
                }
                break;
        } /* end switch() */
    } /* end while() */
} /* end PrintRand() */


PRIntn main(PRIntn argc, char *argv[])
{
    {
        /*
        ** Get command line options
        */
        PLOptStatus os;
        PLOptState *opt = PL_CreateOptState(argc, argv, "hdv");

	    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
        {
		    if (PL_OPT_BAD == os) continue;
            switch (opt->option)
            {
            case 'd':  /* debug */
                debug = 1;
			    msgLevel = PR_LOG_ERROR;
                break;
            case 'v':  /* verbose mode */
			    msgLevel = PR_LOG_DEBUG;
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
    for ( i = 0; i < optRandCount ; i++ ) {
        memset( buf, 0, bufSize );
        rSize = PR_GetRandomNoise( buf, bufSize );
        if (!rSize) {
            fprintf(stderr, "Not implemented\n" );
            failed_already = PR_TRUE;
            break;
        }
        if (debug) PrintRand( buf, rSize );
    }

    if (debug) printf("%s\n", (failed_already)? "FAIL" : "PASS");
    return( (failed_already == PR_TRUE )? 1 : 0 );
}  /* main() */
/* end template.c */

