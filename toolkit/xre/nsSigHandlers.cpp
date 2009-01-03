/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jerry.Kirk@Nexwarecorp.com
 *   Chris Seawood <cls@seawood.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 * This module is supposed to abstract signal handling away from the other
 * platforms that do not support it.
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include "prthread.h"
#include "plstr.h"
#include "prenv.h"
#include "nsDebug.h"

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
#include "nsIAppStartup.h"
#include "nsXPFEComponentsCID.h"
#endif

#ifdef MOZ_WIDGET_PHOTON
#include <photon/PhProto.h>
#endif

static char _progname[1024] = "huh?";
static unsigned int _gdb_sleep_duration = 300;

#if defined(LINUX) && defined(DEBUG) && \
      (defined(__i386) || defined(__x86_64) || defined(PPC))
#define CRAWL_STACK_ON_SIGSEGV
#endif

#ifdef MOZ_WIDGET_PHOTON
void abnormal_exit_handler(int signum)
{
  /* Free any shared memory that has been allocated */
  PgShmemCleanup();

#if defined(DEBUG)
  if (    (signum == SIGSEGV)
       || (signum == SIGILL)
       || (signum == SIGABRT)
       || (signum == SIGFPE)
     )
  {
    printf("prog = %s\npid = %d\nsignal = %s\n", 
           _progname, getpid(), strsignal(signum));

    printf("Sleeping for %d seconds.\n",_gdb_sleep_duration);
    printf("Type 'gdb %s %d' to attach your debugger to this thread.\n",
           _progname, getpid());

    sleep(_gdb_sleep_duration);

    printf("Done sleeping...\n");
  }
#endif

  _exit(1);
}
#elif defined(CRAWL_STACK_ON_SIGSEGV)

#include <unistd.h>
#include "nsISupportsUtils.h"
#include "nsStackWalk.h"

extern "C" {

static void PrintStackFrame(void *aPC, void *aClosure)
{
  char buf[1024];
  nsCodeAddressDetails details;

  NS_DescribeCodeAddress(aPC, &details);
  NS_FormatCodeAddressDetails(aPC, &details, buf, sizeof(buf));
  fprintf(stdout, buf);
}

}

void
ah_crap_handler(int signum)
{
  printf("\nProgram %s (pid = %d) received signal %d.\n",
         _progname,
         getpid(),
         signum);

  printf("Stack:\n");
  NS_StackWalk(PrintStackFrame, 2, nsnull);

  printf("Sleeping for %d seconds.\n",_gdb_sleep_duration);
  printf("Type 'gdb %s %d' to attach your debugger to this thread.\n",
         _progname,
         getpid());

  sleep(_gdb_sleep_duration);

  printf("Done sleeping...\n");
}
#endif // CRAWL_STACK_ON_SIGSEGV

#ifdef XP_BEOS
void beos_signal_handler(int signum) {
#ifdef DEBUG
	fprintf(stderr, "beos_signal_handler: %d\n", signum);
#endif
	nsresult rv;
	nsCOMPtr<nsIAppStartup> appStartup(do_GetService(NS_APPSTARTUP_CONTRACTID, &rv));
	if (NS_FAILED(rv)) {
		// Failed to get the appstartup service so shutdown the hard way
#ifdef DEBUG
		fprintf(stderr, "beos_signal_handler: appShell->do_GetService() failed\n");
#endif
		exit(13);
	}

	// Exit the appshell so that the app can shutdown normally
	appStartup->Quit(nsIAppStartup::eAttemptQuit);
}
#endif

#ifdef MOZ_WIDGET_GTK2
// Need this include for version test below.
#include <glib.h>
#endif

#if defined(MOZ_WIDGET_GTK2) && (GLIB_MAJOR_VERSION > 2 || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 6))

static GLogFunc orig_log_func = NULL;

extern "C" {
static void
my_glib_log_func(const gchar *log_domain, GLogLevelFlags log_level,
                 const gchar *message, gpointer user_data);
}

/* static */ void
my_glib_log_func(const gchar *log_domain, GLogLevelFlags log_level,
                 const gchar *message, gpointer user_data)
{
  if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION)) {
    NS_DebugBreak(NS_DEBUG_ASSERTION, message, "glib assertion", __FILE__, __LINE__);
  } else if (log_level & (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING)) {
    NS_DebugBreak(NS_DEBUG_WARNING, message, "glib warning", __FILE__, __LINE__);
  }

  orig_log_func(log_domain, log_level, message, NULL);
}

#endif

void InstallUnixSignalHandlers(const char *ProgramName)
{
  PL_strncpy(_progname,ProgramName, (sizeof(_progname)-1) );

  const char *gdbSleep = PR_GetEnv("MOZ_GDB_SLEEP");
  if (gdbSleep && *gdbSleep)
  {
    unsigned int s;
    if (1 == sscanf(gdbSleep, "%u", &s)) {
      _gdb_sleep_duration = s;
    }
  }

#if defined(MOZ_WIDGET_PHOTON)
 /* Neutrino need this to free shared memory in case of a crash */
  signal(SIGTERM, abnormal_exit_handler);
  signal(SIGQUIT, abnormal_exit_handler);
  signal(SIGINT,  abnormal_exit_handler);
  signal(SIGHUP,  abnormal_exit_handler);
  signal(SIGSEGV, abnormal_exit_handler);
  signal(SIGILL,  abnormal_exit_handler);
  signal(SIGABRT, abnormal_exit_handler);
  signal(SIGFPE,  abnormal_exit_handler);

#elif defined(CRAWL_STACK_ON_SIGSEGV)
  signal(SIGSEGV, ah_crap_handler);
  signal(SIGILL, ah_crap_handler);
  signal(SIGABRT, ah_crap_handler);
  signal(SIGFPE, ah_crap_handler);
#endif // CRAWL_STACK_ON_SIGSEGV

#if defined(DEBUG) && defined(LINUX)
  const char *memLimit = PR_GetEnv("MOZ_MEM_LIMIT");
  if (memLimit && *memLimit)
  {
    long m = atoi(memLimit);
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

#if defined(MOZ_WIDGET_GTK2) && (GLIB_MAJOR_VERSION > 2 || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 6))
  const char *assertString = PR_GetEnv("XPCOM_DEBUG_BREAK");
  if (assertString &&
      (!strcmp(assertString, "suspend") ||
       !strcmp(assertString, "stack") ||
       !strcmp(assertString, "abort") ||
       !strcmp(assertString, "trap") ||
       !strcmp(assertString, "break"))) {
    // Override the default glib logging function so we get stacks for it too.
    orig_log_func = g_log_set_default_handler(my_glib_log_func, NULL);
  }
#endif
}
