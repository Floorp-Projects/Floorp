/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#include "plgetopt.h"
#include "nspr.h" 
#include "prrng.h"
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


int main(int argc, char **argv)
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

