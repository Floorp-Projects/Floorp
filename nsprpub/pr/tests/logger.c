/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * File:        logger.c
 * Description: test program for logging's basic functions
 */

#include "prinit.h"
#include "prlog.h"
#include "prlock.h"
#include "prcvar.h"
#include "prthread.h"
#include "prinrval.h"

#include <stdio.h>

#ifdef XP_MAC
extern void SetupMacPrintfLog(char *logFile);
#endif

/* lth. re-define PR_LOG() */
#if 0
#undef PR_LOG_TEST
#undef PR_LOG
#define PR_LOG_TEST(_module,_level) ((_module)->level <= (_level))
#define PR_LOG(_module,_level,_args)    \
  {                                     \
    if (PR_LOG_TEST(_module,_level))    \
       PR_LogPrint _args   ;             \
  }
#endif


static void Error(const char* msg)
{
    printf("\t%s\n", msg);
}  /* Error */

static void PR_CALLBACK forked(void *arg)
{
    PRIntn i;
	PRLock *ml;
	PRCondVar *cv;
	
    PR_LogPrint("%s logging creating mutex\n", (const char*)arg);
    ml = PR_NewLock();
    PR_LogPrint("%s logging creating condition variable\n", (const char*)arg);
    cv = PR_NewCondVar(ml);

    PR_LogPrint("%s waiting on condition timeout 10 times\n", (const char*)arg);
    for (i = 0; i < 10; ++i)
    {
        PR_Lock(ml);
        PR_WaitCondVar(cv, PR_SecondsToInterval(1));
        PR_Unlock(ml);
    }
    
    PR_LogPrint("%s logging destroying condition variable\n", (const char*)arg);
    PR_DestroyCondVar(cv);
    PR_LogPrint("%s logging destroying mutex\n", (const char*)arg);
    PR_DestroyLock(ml);
    PR_LogPrint("%s forked thread exiting\n", (const char*)arg);
}

static void UserLogStuff( void )
{
    PRLogModuleInfo *myLM;
    PRIntn i;

    myLM = PR_NewLogModule( "userStuff" );
    if (! myLM )
      {
        printf("UserLogStuff(): can't create new log module\n" );
        return;
      }

    PR_LOG( myLM, PR_LOG_NOTICE, ("Log a Notice %d\n", 1 ));

    for (i = 0; i < 10 ; i++ )
      {
        PR_LOG( myLM, PR_LOG_DEBUG, ("Log Debug number: %d\n", i));
        PR_Sleep( 300 );
      }

} /* end UserLogStuff() */

int main(PRIntn argc, const char **argv)
{
    PRThread *thread;

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

#ifndef XP_MAC
    if (argc > 1)
    {
        if (!PR_SetLogFile(argv[1]))
        {
            Error("Access: Cannot create log file");
            goto exit;
        }
    }
#else
	SetupMacPrintfLog("logger.log");
#endif

    /* Start logging something here */
    PR_LogPrint("%s logging into %s\n", argv[0], argv[1]);

    PR_LogPrint("%s creating new thread\n", argv[0]);

    /*
    ** Now change buffering.
    */
    PR_SetLogBuffering( 65500 );    
	thread = PR_CreateThread(
	    PR_USER_THREAD, forked, (void*)argv[0], PR_PRIORITY_NORMAL,
	    PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    PR_LogPrint("%s joining thread\n", argv[0]);

    UserLogStuff();

    PR_JoinThread(thread);

    PR_LogFlush();
    return 0;

exit:
    return -1;
}

/* logger.c */
