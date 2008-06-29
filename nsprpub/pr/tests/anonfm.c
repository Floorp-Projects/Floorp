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
** File: anonfm.c
** Description: Test anonymous file map 
**
** Synopsis: anonfm [options] [dirName]
**
** Options:
** -d   enable debug mode
** -h   display a help message
** -s <n>  size of the anonymous memory map, in KBytes. default: 100KBytes.
** -C 1 Operate this process as ClientOne() 
** -C 2 Operate this process as ClientTwo()
**
** anonfn.c contains two tests, corresponding to the two protocols for
** passing an anonymous file map to a child process.
**
** ServerOne()/ClientOne() tests the passing of "raw" file map; it uses
** PR_CreateProcess() [for portability of the test case] to create the
** child process, but does not use the PRProcessAttr structure for
** passing the file map data.
**
** ServerTwo()/ClientTwo() tests the passing of the file map using the
** PRProcessAttr structure.
**
*/
#include <plgetopt.h> 
#include <nspr.h> 
#include <private/primpl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
** Test harness infrastructure
*/
PRLogModuleInfo *lm;
PRLogModuleLevel msgLevel = PR_LOG_NONE;
PRUint32  failed_already = 0;

PRIntn  debug = 0;
PRIntn  client = 0; /* invoke client, style */
char    dirName[512] = "."; /* directory name to contain anon mapped file */
PRSize  fmSize = (100 * 1024 );
PRUint32 fmMode = 0600;
PRFileMapProtect fmProt = PR_PROT_READWRITE;
const char *fmEnvName = "nsprFileMapEnvVariable";

/*
** Emit help text for this test
*/
static void Help( void )
{
    printf("anonfm [options] [dirName]\n");
    printf("-d -- enable debug mode\n");
    printf("dirName is alternate directory name. Default: . (current directory)\n");
    exit(1);
} /* end Help() */


/*
** ClientOne() --
*/
static void ClientOne( void )
{
    PRFileMap   *fm;
    char        *fmString;
    char        *addr;
    PRStatus    rc;

    PR_LOG(lm, msgLevel,
        ("ClientOne() starting"));
    
    fmString = PR_GetEnv( fmEnvName );
    if ( NULL == fmString ) {
        failed_already = 1;    
        PR_LOG(lm, msgLevel,
                ("ClientOne(): PR_Getenv() failed"));
        return;
    }
    PR_LOG(lm, msgLevel,
        ("ClientOne(): PR_Getenv(): found: %s", fmString));

    fm = PR_ImportFileMapFromString( fmString );
    if ( NULL == fm ) {
        failed_already = 1;    
        PR_LOG(lm, msgLevel,
                ("ClientOne(): PR_ImportFileMapFromString() failed"));
        return;
    }
    PR_LOG(lm, msgLevel,
        ("ClientOne(): PR_ImportFileMapFromString(): fm: %p", fm ));

    addr = PR_MemMap( fm, LL_ZERO, fmSize );
    if ( NULL == addr ) {
        failed_already = 1;    
        PR_LOG(lm, msgLevel,
            ("ClientOne(): PR_MemMap() failed, OSError: %d", PR_GetOSError() ));
        return;
    }
    PR_LOG(lm, msgLevel,
        ("ClientOne(): PR_MemMap(): addr: %p", addr ));

    /* write to memory map to release server */
    *addr = 1;

    rc = PR_MemUnmap( addr, fmSize );
    PR_ASSERT( rc == PR_SUCCESS );
    PR_LOG(lm, msgLevel,
        ("ClientOne(): PR_MemUnap(): success" ));

    rc = PR_CloseFileMap( fm );
    if ( PR_FAILURE == rc ) {
        failed_already = 1;    
        PR_LOG(lm, msgLevel,
            ("ClientOne(): PR_MemUnap() failed, OSError: %d", PR_GetOSError() ));
        return;
    }
    PR_LOG(lm, msgLevel,
        ("ClientOne(): PR_CloseFileMap(): success" ));

    return;
} /* end ClientOne() */

/*
** ClientTwo() --
*/
static void ClientTwo( void )
{
    failed_already = 1;
} /* end ClientTwo() */

/*
** ServerOne() --
*/
static void ServerOne( void )
{
    PRFileMap   *fm;
    PRStatus    rc;
    PRIntn      i;
    char        *addr;
    char        fmString[256];
    char        envBuf[256];
    char        *child_argv[8];
    PRProcess   *proc;
    PRInt32     exit_status;

    PR_LOG(lm, msgLevel,
        ("ServerOne() starting"));
    
    fm = PR_OpenAnonFileMap( dirName, fmSize, fmProt );
    if ( NULL == fm )      {
        failed_already = 1;    
        PR_LOG(lm, msgLevel,
                ("PR_OpenAnonFileMap() failed"));
        return;
    }
    PR_LOG(lm, msgLevel,
        ("ServerOne(): FileMap: %p", fm ));
    
    rc = PR_ExportFileMapAsString( fm, sizeof(fmString), fmString );
    if ( PR_FAILURE == rc )  {
        failed_already = 1;    
        PR_LOG(lm, msgLevel,
            ("PR_ExportFileMap() failed"));
        return;
    }

    /*
    ** put the string into the environment
    */
    PR_snprintf( envBuf, sizeof(envBuf), "%s=%s", fmEnvName, fmString);
    putenv( envBuf );
    
    addr = PR_MemMap( fm, LL_ZERO, fmSize );
    if ( NULL == addr ) {
        failed_already = 1;    
        PR_LOG(lm, msgLevel,
            ("PR_MemMap() failed"));
        return;
    }

    /* set initial value for client */
    for (i = 0; i < (PRIntn)fmSize ; i++ )
        *(addr+i) = 0x00;  

    PR_LOG(lm, msgLevel,
        ("ServerOne(): PR_MemMap(): addr: %p", addr ));
    
    /*
    ** set arguments for child process
    */
    child_argv[0] = "anonfm";
    child_argv[1] = "-C";
    child_argv[2] = "1";
    child_argv[3] = NULL;

    proc = PR_CreateProcess(child_argv[0], child_argv, NULL, NULL);
    PR_ASSERT( proc );
    PR_LOG(lm, msgLevel,
        ("ServerOne(): PR_CreateProcess(): proc: %x", proc ));

    /*
    ** ClientOne() will set the memory to 1
    */
    PR_LOG(lm, msgLevel,
        ("ServerOne(): waiting on Client, *addr: %x", *addr ));
    while( *addr == 0x00 ) {
        if ( debug )
            fprintf(stderr, ".");
        PR_Sleep(PR_MillisecondsToInterval(300));
    }
    if ( debug )
        fprintf(stderr, "\n");
    PR_LOG(lm, msgLevel,
        ("ServerOne(): Client responded" ));

    rc = PR_WaitProcess( proc, &exit_status );
    PR_ASSERT( PR_FAILURE != rc );

    rc = PR_MemUnmap( addr, fmSize);
    if ( PR_FAILURE == rc ) {
        failed_already = 1;    
        PR_LOG(lm, msgLevel,
            ("PR_MemUnmap() failed"));
        return;
    }
    PR_LOG(lm, msgLevel,
        ("ServerOne(): PR_MemUnmap(): success" ));

    rc = PR_CloseFileMap(fm);
    if ( PR_FAILURE == rc ) {
        failed_already = 1;    
        PR_LOG(lm, msgLevel,
            ("PR_CloseFileMap() failed"));
        return;
    }
    PR_LOG(lm, msgLevel,
        ("ServerOne(): PR_CloseFileMap() success" ));

    return;
} /* end ServerOne() */

/*
** ServerTwo() --
*/
static void ServerTwo( void )
{
    PR_LOG(lm, msgLevel,
        ("ServerTwo(): Not implemented yet" ));
} /* end ServerTwo() */


PRIntn main(PRIntn argc, char *argv[])
{
    {
        /*
        ** Get command line options
        */
        PLOptStatus os;
        PLOptState *opt = PL_CreateOptState(argc, argv, "hdC:");

	    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
        {
		    if (PL_OPT_BAD == os) continue;
            switch (opt->option)
            {
            case 'C':  /* Client style */
                client = atol(opt->value);
                break;
            case 's':  /* file size */
                fmSize = atol( opt->value ) * 1024;
                break;
            case 'd':  /* debug */
                debug = 1;
			    msgLevel = PR_LOG_DEBUG;
                break;
            case 'h':  /* help message */
			    Help();
                break;
             default:
                strcpy(dirName, opt->value);
                break;
            }
        }
	    PL_DestroyOptState(opt);
    }

    lm = PR_NewLogModule("Test");       /* Initialize logging */

    if ( client == 1 ) {
        ClientOne();
    } else if ( client == 2 )  {
        ClientTwo();
    } else {
        ServerOne();
        if ( failed_already ) goto Finished;
        ServerTwo();
    }

Finished:
    if ( debug )
        printf("%s\n", (failed_already)? "FAIL" : "PASS");
    return( (failed_already == PR_TRUE )? 1 : 0 );
}  /* main() */
/* end anonfm.c */

