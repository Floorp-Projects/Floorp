/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Chromium headers must come before Mozilla headers.
#include "base/process_util.h"

#include "mozilla/Atomics.h"

#include "nsDebugImpl.h"
#include "nsDebug.h"
#ifdef MOZ_CRASHREPORTER
# include "nsExceptionHandler.h"
#endif
#include "nsString.h"
#include "nsXULAppAPI.h"
#include "prprf.h"
#include "prlog.h"
#include "nsError.h"
#include "prerror.h"
#include "prerr.h"
#include "prenv.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef _WIN32
/* for getenv() */
#include <stdlib.h>
#endif

#include "nsTraceRefcnt.h"

#if defined(XP_UNIX)
#include <signal.h>
#endif

#if defined(XP_WIN)
#include <tchar.h>
#include "nsString.h"
#ifdef MOZ_METRO
#include "nsWindowsHelpers.h"
#endif
#endif

#if defined(XP_MACOSX) || defined(__DragonFly__) || defined(__FreeBSD__) \
 || defined(__NetBSD__) || defined(__OpenBSD__)
#include <stdbool.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#if defined(__OpenBSD__)
#include <sys/proc.h>
#endif

#if defined(__DragonFly__) || defined(__FreeBSD__)
#include <sys/user.h>
#endif

#if defined(__NetBSD__)
#undef KERN_PROC
#define KERN_PROC KERN_PROC2
#define KINFO_PROC struct kinfo_proc2
#else
#define KINFO_PROC struct kinfo_proc
#endif

#if defined(XP_MACOSX)
#define KP_FLAGS kp_proc.p_flag
#elif defined(__DragonFly__)
#define KP_FLAGS kp_flags
#elif defined(__FreeBSD__)
#define KP_FLAGS ki_flag
#elif defined(__OpenBSD__) && !defined(_P_TRACED)
#define KP_FLAGS p_psflags
#define P_TRACED PS_TRACED
#else
#define KP_FLAGS p_flag
#endif

#include "mozilla/mozalloc_abort.h"

static void
Abort(const char* aMsg);

static void
RealBreak();

static void
Break(const char* aMsg);

#if defined(_WIN32)
#include <windows.h>
#include <signal.h>
#include <malloc.h> // for _alloca
#elif defined(XP_UNIX)
#include <stdlib.h>
#endif

using namespace mozilla;

static const char* sMultiprocessDescription = nullptr;

static Atomic<int32_t> gAssertionCount;

NS_IMPL_QUERY_INTERFACE(nsDebugImpl, nsIDebug, nsIDebug2)

NS_IMETHODIMP_(MozExternalRefCountType)
nsDebugImpl::AddRef()
{
  return 2;
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsDebugImpl::Release()
{
  return 1;
}

NS_IMETHODIMP
nsDebugImpl::Assertion(const char* aStr, const char* aExpr,
                       const char* aFile, int32_t aLine)
{
  NS_DebugBreak(NS_DEBUG_ASSERTION, aStr, aExpr, aFile, aLine);
  return NS_OK;
}

NS_IMETHODIMP
nsDebugImpl::Warning(const char* aStr, const char* aFile, int32_t aLine)
{
  NS_DebugBreak(NS_DEBUG_WARNING, aStr, nullptr, aFile, aLine);
  return NS_OK;
}

NS_IMETHODIMP
nsDebugImpl::Break(const char* aFile, int32_t aLine)
{
  NS_DebugBreak(NS_DEBUG_BREAK, nullptr, nullptr, aFile, aLine);
  return NS_OK;
}

NS_IMETHODIMP
nsDebugImpl::Abort(const char* aFile, int32_t aLine)
{
  NS_DebugBreak(NS_DEBUG_ABORT, nullptr, nullptr, aFile, aLine);
  return NS_OK;
}

NS_IMETHODIMP
nsDebugImpl::GetIsDebugBuild(bool* aResult)
{
#ifdef DEBUG
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsDebugImpl::GetAssertionCount(int32_t* aResult)
{
  *aResult = gAssertionCount;
  return NS_OK;
}

NS_IMETHODIMP
nsDebugImpl::GetIsDebuggerAttached(bool* aResult)
{
  *aResult = false;

#if defined(XP_WIN)
  *aResult = ::IsDebuggerPresent();
#elif defined(XP_MACOSX) || defined(__DragonFly__) || defined(__FreeBSD__) \
   || defined(__NetBSD__) || defined(__OpenBSD__)
  // Specify the info we're looking for
  int mib[] = {
    CTL_KERN,
    KERN_PROC,
    KERN_PROC_PID,
    getpid(),
#if defined(__NetBSD__) || defined(__OpenBSD__)
    sizeof(KINFO_PROC),
    1,
#endif
  };
  u_int mibSize = sizeof(mib) / sizeof(int);

  KINFO_PROC info;
  size_t infoSize = sizeof(info);
  memset(&info, 0, infoSize);

  if (sysctl(mib, mibSize, &info, &infoSize, nullptr, 0)) {
    // if the call fails, default to false
    *aResult = false;
    return NS_OK;
  }

  if (info.KP_FLAGS & P_TRACED) {
    *aResult = true;
  }
#endif

  return NS_OK;
}

/* static */ void
nsDebugImpl::SetMultiprocessMode(const char* aDesc)
{
  sMultiprocessDescription = aDesc;
}

/**
 * Implementation of the nsDebug methods. Note that this code is
 * always compiled in, in case some other module that uses it is
 * compiled with debugging even if this library is not.
 */
static PRLogModuleInfo* gDebugLog;

static void
InitLog()
{
  if (0 == gDebugLog) {
    gDebugLog = PR_NewLogModule("nsDebug");
  }
}

enum nsAssertBehavior
{
  NS_ASSERT_UNINITIALIZED,
  NS_ASSERT_WARN,
  NS_ASSERT_SUSPEND,
  NS_ASSERT_STACK,
  NS_ASSERT_TRAP,
  NS_ASSERT_ABORT,
  NS_ASSERT_STACK_AND_ABORT
};

static nsAssertBehavior
GetAssertBehavior()
{
  static nsAssertBehavior gAssertBehavior = NS_ASSERT_UNINITIALIZED;
  if (gAssertBehavior != NS_ASSERT_UNINITIALIZED) {
    return gAssertBehavior;
  }

  gAssertBehavior = NS_ASSERT_WARN;

  const char* assertString = PR_GetEnv("XPCOM_DEBUG_BREAK");
  if (!assertString || !*assertString) {
    return gAssertBehavior;
  }
  if (!strcmp(assertString, "warn")) {
    return gAssertBehavior = NS_ASSERT_WARN;
  }
  if (!strcmp(assertString, "suspend")) {
    return gAssertBehavior = NS_ASSERT_SUSPEND;
  }
  if (!strcmp(assertString, "stack")) {
    return gAssertBehavior = NS_ASSERT_STACK;
  }
  if (!strcmp(assertString, "abort")) {
    return gAssertBehavior = NS_ASSERT_ABORT;
  }
  if (!strcmp(assertString, "trap") || !strcmp(assertString, "break")) {
    return gAssertBehavior = NS_ASSERT_TRAP;
  }
  if (!strcmp(assertString, "stack-and-abort")) {
    return gAssertBehavior = NS_ASSERT_STACK_AND_ABORT;
  }

  fprintf(stderr, "Unrecognized value of XPCOM_DEBUG_BREAK\n");
  return gAssertBehavior;
}

struct FixedBuffer
{
  FixedBuffer() : curlen(0)
  {
    buffer[0] = '\0';
  }

  char buffer[1000];
  uint32_t curlen;
};

static int
StuffFixedBuffer(void* aClosure, const char* aBuf, uint32_t aLen)
{
  if (!aLen) {
    return 0;
  }

  FixedBuffer* fb = (FixedBuffer*)aClosure;

  // strip the trailing null, we add it again later
  if (aBuf[aLen - 1] == '\0') {
    --aLen;
  }

  if (fb->curlen + aLen >= sizeof(fb->buffer)) {
    aLen = sizeof(fb->buffer) - fb->curlen - 1;
  }

  if (aLen) {
    memcpy(fb->buffer + fb->curlen, aBuf, aLen);
    fb->curlen += aLen;
    fb->buffer[fb->curlen] = '\0';
  }

  return aLen;
}

EXPORT_XPCOM_API(void)
NS_DebugBreak(uint32_t aSeverity, const char* aStr, const char* aExpr,
              const char* aFile, int32_t aLine)
{
  InitLog();

  FixedBuffer buf;
  PRLogModuleLevel ll = PR_LOG_WARNING;
  const char* sevString = "WARNING";

  switch (aSeverity) {
    case NS_DEBUG_ASSERTION:
      sevString = "###!!! ASSERTION";
      ll = PR_LOG_ERROR;
      break;

    case NS_DEBUG_BREAK:
      sevString = "###!!! BREAK";
      ll = PR_LOG_ALWAYS;
      break;

    case NS_DEBUG_ABORT:
      sevString = "###!!! ABORT";
      ll = PR_LOG_ALWAYS;
      break;

    default:
      aSeverity = NS_DEBUG_WARNING;
  };

#  define PrintToBuffer(...) PR_sxprintf(StuffFixedBuffer, &buf, __VA_ARGS__)

  // Print "[PID]" or "[Desc PID]" at the beginning of the message.
  PrintToBuffer("[");
  if (sMultiprocessDescription) {
    PrintToBuffer("%s ", sMultiprocessDescription);
  }
  PrintToBuffer("%d] ", base::GetCurrentProcId());

  PrintToBuffer("%s: ", sevString);

  if (aStr) {
    PrintToBuffer("%s: ", aStr);
  }
  if (aExpr) {
    PrintToBuffer("'%s', ", aExpr);
  }
  if (aFile) {
    PrintToBuffer("file %s, ", aFile);
  }
  if (aLine != -1) {
    PrintToBuffer("line %d", aLine);
  }

#  undef PrintToBuffer

  // Write out the message to the debug log
  PR_LOG(gDebugLog, ll, ("%s", buf.buffer));
  PR_LogFlush();

  // errors on platforms without a debugdlg ring a bell on stderr
#if !defined(XP_WIN)
  if (ll != PR_LOG_WARNING) {
    fprintf(stderr, "\07");
  }
#endif

#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", buf.buffer);
#endif

  // Write the message to stderr unless it's a warning and MOZ_IGNORE_WARNINGS
  // is set.
  if (!(PR_GetEnv("MOZ_IGNORE_WARNINGS") && aSeverity == NS_DEBUG_WARNING)) {
    fprintf(stderr, "%s\n", buf.buffer);
    fflush(stderr);
  }

  switch (aSeverity) {
    case NS_DEBUG_WARNING:
      return;

    case NS_DEBUG_BREAK:
      Break(buf.buffer);
      return;

    case NS_DEBUG_ABORT: {
#if defined(MOZ_CRASHREPORTER)
      // Updating crash annotations in the child causes us to do IPC. This can
      // really cause trouble if we're asserting from within IPC code. So we
      // have to do without the annotations in that case.
      if (XRE_GetProcessType() == GeckoProcessType_Default) {
        nsCString note("xpcom_runtime_abort(");
        note += buf.buffer;
        note += ")";
        CrashReporter::AppendAppNotesToCrashReport(note);
        CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AbortMessage"),
                                           nsDependentCString(buf.buffer));
      }
#endif  // MOZ_CRASHREPORTER

#if defined(DEBUG) && defined(_WIN32)
      RealBreak();
#endif
#ifdef DEBUG
      nsTraceRefcnt::WalkTheStack(stderr);
#endif
      Abort(buf.buffer);
      return;
    }
  }

  // Now we deal with assertions
  gAssertionCount++;

  switch (GetAssertBehavior()) {
    case NS_ASSERT_WARN:
      return;

    case NS_ASSERT_SUSPEND:
#ifdef XP_UNIX
      fprintf(stderr, "Suspending process; attach with the debugger.\n");
      kill(0, SIGSTOP);
#else
      Break(buf.buffer);
#endif
      return;

    case NS_ASSERT_STACK:
      nsTraceRefcnt::WalkTheStack(stderr);
      return;

    case NS_ASSERT_STACK_AND_ABORT:
      nsTraceRefcnt::WalkTheStack(stderr);
      // Fall through to abort

    case NS_ASSERT_ABORT:
      Abort(buf.buffer);
      return;

    case NS_ASSERT_TRAP:
    case NS_ASSERT_UNINITIALIZED: // Default to "trap" behavior
      Break(buf.buffer);
      return;
  }
}

static void
Abort(const char* aMsg)
{
  mozalloc_abort(aMsg);
}

static void
RealBreak()
{
#if defined(_WIN32)
  ::DebugBreak();
#elif defined(XP_MACOSX)
  raise(SIGTRAP);
#elif defined(__GNUC__) && (defined(__i386__) || defined(__i386) || defined(__x86_64__))
  asm("int $3");
#elif defined(__arm__)
  asm(
#ifdef __ARM_ARCH_4T__
    /* ARMv4T doesn't support the BKPT instruction, so if the compiler target
     * is ARMv4T, we want to ensure the assembler will understand that ARMv5T
     * instruction, while keeping the resulting object tagged as ARMv4T.
     */
    ".arch armv5t\n"
    ".object_arch armv4t\n"
#endif
    "BKPT #0");
#elif defined(SOLARIS)
#if defined(__i386__) || defined(__i386) || defined(__x86_64__)
  asm("int $3");
#else
  raise(SIGTRAP);
#endif
#else
#warning do not know how to break on this platform
#endif
}

// Abort() calls this function, don't call it!
static void
Break(const char* aMsg)
{
#if defined(_WIN32)
  static int ignoreDebugger;
  if (!ignoreDebugger) {
    const char* shouldIgnoreDebugger = getenv("XPCOM_DEBUG_DLG");
    ignoreDebugger =
      1 + (shouldIgnoreDebugger && !strcmp(shouldIgnoreDebugger, "1"));
  }
  if ((ignoreDebugger == 2) || !::IsDebuggerPresent()) {
    DWORD code = IDRETRY;

    /* Create the debug dialog out of process to avoid the crashes caused by
     * Windows events leaking into our event loop from an in process dialog.
     * We do this by launching windbgdlg.exe (built in xpcom/windbgdlg).
     * See http://bugzilla.mozilla.org/show_bug.cgi?id=54792
     */
    PROCESS_INFORMATION pi;
    STARTUPINFOW si;
    wchar_t executable[MAX_PATH];
    wchar_t* pName;

    memset(&pi, 0, sizeof(pi));

    memset(&si, 0, sizeof(si));
    si.cb          = sizeof(si);
    si.wShowWindow = SW_SHOW;

    // 2nd arg of CreateProcess is in/out
    wchar_t* msgCopy = (wchar_t*)_alloca((strlen(aMsg) + 1) * sizeof(wchar_t));
    wcscpy(msgCopy, NS_ConvertUTF8toUTF16(aMsg).get());

    if (GetModuleFileNameW(GetModuleHandleW(L"xpcom.dll"), executable, MAX_PATH) &&
        (pName = wcsrchr(executable, '\\')) != nullptr &&
        wcscpy(pName + 1, L"windbgdlg.exe") &&
        CreateProcessW(executable, msgCopy, nullptr, nullptr,
                       false, DETACHED_PROCESS | NORMAL_PRIORITY_CLASS,
                       nullptr, nullptr, &si, &pi)) {
      WaitForSingleObject(pi.hProcess, INFINITE);
      GetExitCodeProcess(pi.hProcess, &code);
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
    }

    switch (code) {
      case IDABORT:
        //This should exit us
        raise(SIGABRT);
        //If we are ignored exit this way..
        _exit(3);

      case IDIGNORE:
        return;
    }
  }

  RealBreak();
#elif defined(XP_MACOSX)
  /* Note that we put this Mac OS X test above the GNUC/x86 test because the
   * GNUC/x86 test is also true on Intel Mac OS X and we want the PPC/x86
   * impls to be the same.
   */
  RealBreak();
#elif defined(__GNUC__) && (defined(__i386__) || defined(__i386) || defined(__x86_64__))
  RealBreak();
#elif defined(__arm__)
  RealBreak();
#elif defined(SOLARIS)
  RealBreak();
#else
#warning do not know how to break on this platform
#endif
}

static const nsDebugImpl kImpl;

nsresult
nsDebugImpl::Create(nsISupports* aOuter, const nsIID& aIID, void** aInstancePtr)
{
  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }

  return const_cast<nsDebugImpl*>(&kImpl)->QueryInterface(aIID, aInstancePtr);
}

////////////////////////////////////////////////////////////////////////////////

nsresult
NS_ErrorAccordingToNSPR()
{
  PRErrorCode err = PR_GetError();
  switch (err) {
    case PR_OUT_OF_MEMORY_ERROR:         return NS_ERROR_OUT_OF_MEMORY;
    case PR_WOULD_BLOCK_ERROR:           return NS_BASE_STREAM_WOULD_BLOCK;
    case PR_FILE_NOT_FOUND_ERROR:        return NS_ERROR_FILE_NOT_FOUND;
    case PR_READ_ONLY_FILESYSTEM_ERROR:  return NS_ERROR_FILE_READ_ONLY;
    case PR_NOT_DIRECTORY_ERROR:         return NS_ERROR_FILE_NOT_DIRECTORY;
    case PR_IS_DIRECTORY_ERROR:          return NS_ERROR_FILE_IS_DIRECTORY;
    case PR_LOOP_ERROR:                  return NS_ERROR_FILE_UNRESOLVABLE_SYMLINK;
    case PR_FILE_EXISTS_ERROR:           return NS_ERROR_FILE_ALREADY_EXISTS;
    case PR_FILE_IS_LOCKED_ERROR:        return NS_ERROR_FILE_IS_LOCKED;
    case PR_FILE_TOO_BIG_ERROR:          return NS_ERROR_FILE_TOO_BIG;
    case PR_NO_DEVICE_SPACE_ERROR:       return NS_ERROR_FILE_NO_DEVICE_SPACE;
    case PR_NAME_TOO_LONG_ERROR:         return NS_ERROR_FILE_NAME_TOO_LONG;
    case PR_DIRECTORY_NOT_EMPTY_ERROR:   return NS_ERROR_FILE_DIR_NOT_EMPTY;
    case PR_NO_ACCESS_RIGHTS_ERROR:      return NS_ERROR_FILE_ACCESS_DENIED;
    default:                             return NS_ERROR_FAILURE;
  }
}

void
NS_ABORT_OOM(size_t aSize)
{
#ifdef MOZ_CRASHREPORTER
  CrashReporter::AnnotateOOMAllocationSize(aSize);
#endif
  MOZ_CRASH();
}

