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

#include "nsSigHandlers.h"

#ifdef XP_UNIX

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include "prthread.h"
#include "plstr.h"
#include "prenv.h"
#include "nsDebug.h"
#include "nsXULAppAPI.h"

#if defined(LINUX)
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdlib.h> // atoi
#include <ucontext.h>
#endif

#if defined(SOLARIS)
#include <sys/resource.h>
#include <ucontext.h>
#endif

static char _progname[1024] = "huh?";
static unsigned int _gdb_sleep_duration = 300;

// NB: keep me up to date with the same variable in
// ipc/chromium/chrome/common/ipc_channel_posix.cc
static const int kClientChannelFd = 3;

#if defined(LINUX) && defined(DEBUG) && \
      (defined(__i386) || defined(__x86_64) || defined(PPC))
#define CRAWL_STACK_ON_SIGSEGV
#endif

#if defined(CRAWL_STACK_ON_SIGSEGV)

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
  fputs(buf, stdout);
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

  _exit(signum);
}

void
child_ah_crap_handler(int signum)
{
  if (!getenv("MOZ_DONT_UNBLOCK_PARENT_ON_CHILD_CRASH"))
    close(kClientChannelFd);
  ah_crap_handler(signum);
}

#endif // CRAWL_STACK_ON_SIGSEGV

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

#ifdef SA_SIGINFO
static void fpehandler(int signum, siginfo_t *si, void *context)
{
  /* Integer divide by zero or integer overflow. */
  /* Note: FPE_INTOVF is ignored on Intel, PowerPC and SPARC systems. */
  if (si->si_code == FPE_INTDIV || si->si_code == FPE_INTOVF) {
    NS_DebugBreak(NS_DEBUG_ABORT, "Divide by zero", nsnull, __FILE__, __LINE__);
  }

#ifdef XP_MACOSX
  ucontext_t *uc = (ucontext_t *)context;

#if defined(__i386__) || defined(__amd64__)
  _STRUCT_FP_CONTROL *ctrl = &uc->uc_mcontext->__fs.__fpu_fcw;
  ctrl->__invalid = ctrl->__denorm = ctrl->__zdiv = ctrl->__ovrfl = ctrl->__undfl = ctrl->__precis = 1;

  _STRUCT_FP_STATUS *status = &uc->uc_mcontext->__fs.__fpu_fsw;
  status->__invalid = status->__denorm = status->__zdiv = status->__ovrfl = status->__undfl =
    status->__precis = status->__stkflt = status->__errsumm = 0;

  __uint32_t *mxcsr = &uc->uc_mcontext->__fs.__fpu_mxcsr;
  *mxcsr |= SSE_EXCEPTION_MASK; /* disable all SSE exceptions */
  *mxcsr &= ~SSE_STATUS_FLAGS; /* clear all pending SSE exceptions */
#endif
#endif
#ifdef LINUX
  ucontext_t *uc = (ucontext_t *)context;

#if defined(__i386__)
  /*
   * It seems that we have no access to mxcsr on Linux. libc
   * seems to be translating cw/sw to mxcsr.
   */
  unsigned long int *cw = &uc->uc_mcontext.fpregs->cw;
  *cw |= FPU_EXCEPTION_MASK;

  unsigned long int *sw = &uc->uc_mcontext.fpregs->sw;
  *sw &= ~FPU_STATUS_FLAGS;
#endif
#if defined(__amd64__)
  __uint16_t *cw = &uc->uc_mcontext.fpregs->cwd;
  *cw |= FPU_EXCEPTION_MASK;

  __uint16_t *sw = &uc->uc_mcontext.fpregs->swd;
  *sw &= ~FPU_STATUS_FLAGS;

  __uint32_t *mxcsr = &uc->uc_mcontext.fpregs->mxcsr;
  *mxcsr |= SSE_EXCEPTION_MASK; /* disable all SSE exceptions */
  *mxcsr &= ~SSE_STATUS_FLAGS; /* clear all pending SSE exceptions */
#endif
#endif
#ifdef SOLARIS
  ucontext_t *uc = (ucontext_t *)context;

#if defined(__i386)
  uint32_t *cw = &uc->uc_mcontext.fpregs.fp_reg_set.fpchip_state.state[0];
  *cw |= FPU_EXCEPTION_MASK;

  uint32_t *sw = &uc->uc_mcontext.fpregs.fp_reg_set.fpchip_state.state[1];
  *sw &= ~FPU_STATUS_FLAGS;

  /* address of the instruction that caused the exception */
  uint32_t *ip = &uc->uc_mcontext.fpregs.fp_reg_set.fpchip_state.state[3];
  uc->uc_mcontext.gregs[REG_PC] = *ip;
#endif
#if defined(__amd64__)
  uint16_t *cw = &uc->uc_mcontext.fpregs.fp_reg_set.fpchip_state.cw;
  *cw |= FPU_EXCEPTION_MASK;

  uint16_t *sw = &uc->uc_mcontext.fpregs.fp_reg_set.fpchip_state.sw;
  *sw &= ~FPU_STATUS_FLAGS;

  uint32_t *mxcsr = &uc->uc_mcontext.fpregs.fp_reg_set.fpchip_state.mxcsr;
  *mxcsr |= SSE_EXCEPTION_MASK; /* disable all SSE exceptions */
  *mxcsr &= ~SSE_STATUS_FLAGS; /* clear all pending SSE exceptions */
#endif
#endif
}
#endif

void InstallSignalHandlers(const char *ProgramName)
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

#if defined(CRAWL_STACK_ON_SIGSEGV)
  if (!getenv("XRE_NO_WINDOWS_CRASH_DIALOG")) {
    void (*crap_handler)(int) =
      GeckoProcessType_Default != XRE_GetProcessType() ?
          child_ah_crap_handler :
          ah_crap_handler;
    signal(SIGSEGV, crap_handler);
    signal(SIGILL, crap_handler);
    signal(SIGABRT, crap_handler);
  }
#endif // CRAWL_STACK_ON_SIGSEGV

#ifdef SA_SIGINFO
  /* Install a handler for floating point exceptions and disable them if they occur. */
  struct sigaction sa, osa;
  sa.sa_flags = SA_ONSTACK | SA_RESTART | SA_SIGINFO;
  sa.sa_sigaction = fpehandler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGFPE, &sa, &osa);
#endif

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

#elif XP_WIN

#include <windows.h>

#ifdef _M_IX86
/*
 * WinNT.h prior to SDK7 does not expose the structure of the ExtendedRegisters for ia86.
 * We known that MxCsr is at offset 0x18 and is a DWORD.
 */
#define MXCSR(ctx) (*(DWORD *)(((BYTE *)(ctx)->ExtendedRegisters) + 0x18))
#endif

#ifdef _M_X64
#define MXCSR(ctx) (ctx)->MxCsr
#endif

#if defined(_M_IX86) || defined(_M_X64)

#ifdef _M_X64
#define X87CW(ctx) (ctx)->FltSave.ControlWord
#define X87SW(ctx) (ctx)->FltSave.StatusWord
#else
#define X87CW(ctx) (ctx)->FloatSave.ControlWord
#define X87SW(ctx) (ctx)->FloatSave.StatusWord
#endif

/*
 * SSE traps raise these exception codes, which are defined in internal NT headers
 * but not winbase.h
 */
#define STATUS_FLOAT_MULTIPLE_FAULTS 0xC00002B4
#define STATUS_FLOAT_MULTIPLE_TRAPS  0xC00002B5

static LPTOP_LEVEL_EXCEPTION_FILTER gFPEPreviousFilter;

LONG __stdcall FpeHandler(PEXCEPTION_POINTERS pe)
{
  PEXCEPTION_RECORD e = (PEXCEPTION_RECORD)pe->ExceptionRecord;
  CONTEXT *c = (CONTEXT*)pe->ContextRecord;

  switch (e->ExceptionCode) {
    case STATUS_FLOAT_DENORMAL_OPERAND:
    case STATUS_FLOAT_DIVIDE_BY_ZERO:
    case STATUS_FLOAT_INEXACT_RESULT:
    case STATUS_FLOAT_INVALID_OPERATION:
    case STATUS_FLOAT_OVERFLOW:
    case STATUS_FLOAT_STACK_CHECK:
    case STATUS_FLOAT_UNDERFLOW:
    case STATUS_FLOAT_MULTIPLE_FAULTS:
    case STATUS_FLOAT_MULTIPLE_TRAPS:
      X87CW(c) |= FPU_EXCEPTION_MASK; /* disable all FPU exceptions */
      X87SW(c) &= ~FPU_STATUS_FLAGS;  /* clear all pending FPU exceptions */
#ifdef _M_IX86
      if (c->ContextFlags & CONTEXT_EXTENDED_REGISTERS) {
#endif
        MXCSR(c) |= SSE_EXCEPTION_MASK; /* disable all SSE exceptions */
        MXCSR(c) &= ~SSE_STATUS_FLAGS;  /* clear all pending SSE exceptions */
#ifdef _M_IX86
      }
#endif
      return EXCEPTION_CONTINUE_EXECUTION;
  }
  LONG action = EXCEPTION_CONTINUE_SEARCH;
  if (gFPEPreviousFilter)
    action = gFPEPreviousFilter(pe);

  return action;
}

void InstallSignalHandlers(const char *ProgramName)
{
  gFPEPreviousFilter = SetUnhandledExceptionFilter(FpeHandler);
}

#else

void InstallSignalHandlers(const char *ProgramName)
{
}

#endif

#elif defined(XP_OS2)
/* OS/2's FPE handler is implemented in NSPR */

#else
#error No signal handling implementation for this platform.
#endif
