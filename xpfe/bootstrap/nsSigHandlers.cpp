/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Jerry.Kirk@Nexwarecorp.com
 *  Chris Seawood <cls@seawood.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This module is supposed to abstract signal handling away from the other
 * platforms that do not support it.
 */

#include <signal.h>
#include <stdio.h>
#include "prthread.h"
#include "plstr.h"
#include "prenv.h"

#if defined(LINUX)
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdlib.h> // atoi
#endif

#if defined(SOLARIS)
#include <sys/resource.h>
#endif

#ifdef XP_BEOS
#include <be/app/Application.h>
#include <string.h>
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
static NS_DEFINE_CID(kAppShellServiceCID,   NS_APPSHELL_SERVICE_CID);
#else
extern "C" char * strsignal(int);
#endif

#ifdef NTO
#include <photon/PhProto.h>
#include <sys/mman.h>			/* for munlockall() */
#endif

static char _progname[1024] = "huh?";

//#ifdef DEBUG_ramiro
#if 0
#define CRAWL_STACK_ON_SIGSEGV
#endif // DEBUG_ramiro
 
#ifdef NTO
void abnormal_exit_handler(int signum)
{
  /* Free any shared memory that has been allocated */
  PgShmemCleanup();

#if defined(DEBUG)
  if (    (signum == SIGSEGV)
       || (signum == SIGILL)
	   || (signum == SIGABRT)
	 )
  {
    PR_CurrentThread();
    printf("prog = %s\npid = %d\nsignal = %s\n", 
	  _progname, getpid(), strsignal(signum));

    printf("Sleeping for 5 minutes.\n");
    printf("Type 'gdb %s %d' to attatch your debugger to this thread.\n",
	  _progname, getpid());

    sleep(300);

    printf("Done sleeping...\n");
  }
#endif

  _exit(1);
} 
#elif defined(CRAWL_STACK_ON_SIGSEGV)

#include <unistd.h>
#include "nsTraceRefcnt.h"

void
ah_crap_handler(int signum)
{
  PR_CurrentThread();

  printf("prog = %s\npid = %d\nsignal = %s\n",
         _progname,
         getpid(),
         strsignal(signum));
  
  printf("stack logged to someplace\n");
  nsTraceRefcnt::WalkTheStack(stdout);

  printf("Sleeping for 5 minutes.\n");
  printf("Type 'gdb %s %d' to attatch your debugger to this thread.\n",
         _progname,
         getpid());

  sleep(300);

  printf("Done sleeping...\n");
} 
#endif // CRAWL_STACK_ON_SIGSEGV

#ifdef XP_BEOS
void beos_signal_handler(int signum) {
#ifdef DEBUG
	fprintf(stderr, "beos_signal_handler: %d\n", signum);
#endif
	nsresult rv;
	nsCOMPtr<nsIAppShellService> appShellService(do_GetService(kAppShellServiceCID,&rv));
	if (NS_FAILED(rv)) {
		// Failed to get the appshell service so shutdown the hard way
#ifdef DEBUG
		fprintf(stderr, "beos_signal_handler: appShell->do_GetService() failed\n");
#endif
		exit(13);
	}

	// Exit the appshell so that the app can shutdown normally
	appShellService->Quit();
}
#endif 

void InstallUnixSignalHandlers(const char *ProgramName)
{

  PL_strncpy(_progname,ProgramName, (sizeof(_progname)-1) );

#if defined(NTO)
 /* Neutrino need this to free shared memory in case of a crash */
  signal(SIGTERM, abnormal_exit_handler);
  signal(SIGQUIT, abnormal_exit_handler);
  signal(SIGINT,  abnormal_exit_handler);
  signal(SIGHUP,  abnormal_exit_handler);
  signal(SIGSEGV, abnormal_exit_handler);
  signal(SIGILL,  abnormal_exit_handler);
  signal(SIGABRT, abnormal_exit_handler);

/* Tell the OS it can page any part of this program to virtual memory */
  munlockall();
#elif defined(CRAWL_STACK_ON_SIGSEGV)
  signal(SIGSEGV, ah_crap_handler);
  signal(SIGILL, ah_crap_handler);
  signal(SIGABRT, ah_crap_handler);
#endif // CRAWL_STACK_ON_SIGSEGV

#if defined(DEBUG) && defined(LINUX)
  char *text = PR_GetEnv("MOZ_MEM_LIMIT");
  if (text) 
  {
    long m = atoi(text);
    m *= (1024*1024);    
    struct rlimit r;
    r.rlim_cur = m;
    r.rlim_max = m;
    setrlimit(RLIMIT_AS, &r);
  }
#endif

#if defined(SOLARIS)

    #define NOFILES 512

    // Boost Solaris file descriptors
    {
	struct rlimit rl;
	
	if (getrlimit(RLIMIT_NOFILE, &rl) == 0)

	    if (rl.rlim_cur < NOFILES) {
		rl.rlim_cur = NOFILES;

		if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
		    perror("setrlimit(RLIMIT_NOFILE)");
		    fprintf(stderr, "Cannot exceed hard limit for open files");
		}
#if defined(DEBUG)
	    	if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
		    printf("File descriptors set to %d\n", rl.rlim_cur);
#endif //DEBUG
	    }
    }
#endif //SOLARIS

#ifdef XP_BEOS
	signal(SIGTERM, beos_signal_handler);
#endif
}
