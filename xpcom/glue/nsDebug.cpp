/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date        Modified by     Description of modification
 * 04/10/2000  IBM Corp.       Added DebugBreak() definitions for OS/2
 * 06/19/2000  IBM Corp.       Fix DebugBreak() messagebox defaults for OS/2 
 * 06/19/2000  Henry Sobotka Fix DebugBreak() for OS/2 on retail build.
 */

#include "nsDebug.h"
#include "prprf.h"
#include "prlog.h"
#include "prinit.h"
#include "plstr.h"
#include "nsError.h"
#include "prerror.h"
#include "prerr.h"

#if defined(XP_BEOS)
/* For DEBUGGER macros */
#include <Debug.h>
/* for getenv() */
#include <stdlib.h>
#endif

#if defined(XP_UNIX) || defined(_WIN32) || defined(XP_OS2)
/* for abort() and getenv() */
#include <stdlib.h>
#endif

#if defined(XP_UNIX) && !defined(UNIX_CRASH_ON_ASSERT)
#include <signal.h>
/* for nsTraceRefcnt::WalkTheStack() */
#include "nsISupportsUtils.h"
#include "nsTraceRefcnt.h"
#endif

#if defined(XP_OS2)
/* Added definitions for DebugBreak() for 2 different OS/2 compilers.  Doing
 * the int3 on purpose for Visual Age so that a developer can step over the
 * instruction if so desired.  Not always possible if trapping due to exception
 * handling IBM-AKR
 */
#define INCL_WINDIALOGS  // need for WinMessageBox
#include <os2.h>

#if defined(DEBUG)
#if defined(XP_OS2_VACPP)
   #include <builtin.h>
   #define DebugBreak() { _interrupt(3); }
#elif defined(XP_OS2_EMX)
   /* Force a trap */
   #define DebugBreak() { int *pTrap=NULL; *pTrap = 1; }
#else
   #define DebugBreak()
#endif

#else
   #define DebugBreak()
#endif /* DEBUG */
#endif /* XP_OS2 */

#if defined(_WIN32)
#include <windows.h>
#include <signal.h>
#elif defined(XP_MAC)
	   #include <MacTypes.h>
	   #include <Processes.h>
	   #include <string.h>

	   // TEMPORARY UNTIL WE HAVE MACINTOSH ENVIRONMENT VARIABLES THAT CAN TURN ON
	   // LOGGING ON MACINTOSH
	   // At this moment, NSPR's logging is a no-op on Macintosh.

	   #include <stdarg.h>
	   #include <stdio.h>
	 
	   static void dprintf(const char *format, ...)
	   {
	      va_list ap;
	      Str255 buffer;
	      
	      va_start(ap, format);
	      buffer[0] = std::vsnprintf((char *)buffer + 1, sizeof(buffer) - 1, format, ap);
	      va_end(ap);
	      if (PL_strcasestr((char *)&buffer[1], "warning"))
	 	      printf("еее%s\n", (char*)buffer + 1);
	 	  else
	 	      DebugStr(buffer);
	   }
#elif defined(XP_UNIX)
#include<stdlib.h>
#endif

#include "nslog.h"
#undef fprintf
#undef ERROR
#undef WARN
#undef BREAK

NS_IMPL_LOG_ENABLED(ASSERT)
#define ASSERT_PRINTF NS_LOG_PRINTF(ASSERT)
#define ASSERT_FLUSH  NS_LOG_FLUSH(ASSERT)

NS_IMPL_LOG_ENABLED(ABORT)
#define ABORT_PRINTF NS_LOG_PRINTF(ABORT)
#define ABORT_FLUSH  NS_LOG_FLUSH(ABORT)

NS_IMPL_LOG_ENABLED(WARN)
#define WARN_PRINTF NS_LOG_PRINTF(WARN)
#define WARN_FLUSH  NS_LOG_FLUSH(WARN)

NS_IMPL_LOG_ENABLED(BREAK)
#define BREAK_PRINTF NS_LOG_PRINTF(BREAK)
#define BREAK_FLUSH  NS_LOG_FLUSH(BREAK)

/*
 * Determine if debugger is present in windows.
 */
#if defined (_WIN32)

typedef WINBASEAPI BOOL (WINAPI* LPFNISDEBUGGERPRESENT)();
PRBool InDebugger()
{
   PRBool fReturn = PR_FALSE;
   LPFNISDEBUGGERPRESENT lpfnIsDebuggerPresent = NULL;
   HINSTANCE hKernel = LoadLibrary("Kernel32.dll");

   if(hKernel)
      {
      lpfnIsDebuggerPresent = 
         (LPFNISDEBUGGERPRESENT)GetProcAddress(hKernel, "IsDebuggerPresent");
      if(lpfnIsDebuggerPresent)
         {
         fReturn = (*lpfnIsDebuggerPresent)();
         }
      FreeLibrary(hKernel);
      }

   return fReturn;
}

#endif /* WIN32*/


/**
 * Implementation of the nsDebug methods. Note that this code is
 * always compiled in, in case some other module that uses it is
 * compiled with debugging even if this library is not.
 */

NS_COM void nsDebug::Assertion(const char* aStr, const char* aExpr,
                               const char* aFile, PRIntn aLine)
{
   char buf[1000];
   PR_snprintf(buf, sizeof(buf),
               "%s: '%s', file %s, line %d",
               aStr, aExpr, aFile, aLine);

   ASSERT_PRINTF("%s", buf);
   ASSERT_FLUSH();

#if defined(_WIN32)
   if(!InDebugger())
   {
     char msg[1200];
     PR_snprintf(msg, sizeof(msg),
                 "%s\n\nClick Abort to exit the Application.\n"
                 "Click Retry to Debug the Application..\n"
                 "Click Ignore to continue running the Application.", buf); 
     int code = ::MessageBox(NULL, msg, "ASSERTION",
                             MB_ICONSTOP | MB_ABORTRETRYIGNORE);
     switch(code)
     {
       case IDABORT:
         //This should exit us
         raise(SIGABRT);
         //If we are ignored exit this way..
         _exit(3);
         break;
         
       case IDIGNORE:
         return;
         // Fall Through
     }
   }
#endif

#if defined(XP_OS2)
      char msg[1200];
      PR_snprintf(msg, sizeof(msg),
                "%s\n\nClick Cancel to Debug Application.\n"
                "Click Enter to continue running the Application.", buf);
      ULONG code = WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, msg, 
                                 "ASSERTION", 0,
                                 MB_ERROR | MB_ENTERCANCEL);

      /* It is possible that we are executing on a thread that doesn't have a
       * message queue.  In that case, the message won't appear, and code will
       * be 0xFFFF.  We'll give the user a chance to debug it by calling
       * Break()
       * Actually, that's a really bad idea since this happens a lot with threadsafe
       * assertions and since it means that you can't actually run the debug build
       * outside a debugger without it crashing constantly.
       */
      if(( code == MBID_ENTER ) || (code == MBID_ERROR))
      {
         return;
         // If Retry, Fall Through
      }
#endif

#if defined(XP_MAC)
   dprintf("ASSERT: %s", buf);
#else
   Break(aFile, aLine);
#endif
}

NS_COM void nsDebug::Break(const char* aFile, PRIntn aLine)
{
    BREAK_PRINTF("at file %s, line %d", aFile, aLine);
    BREAK_FLUSH();

#ifdef XP_MAC
    dprintf("BREAK: at file %s, line %d", aFile, aLine);
#elif defined(_WIN32)
#ifdef _M_IX86
    ::DebugBreak();
#else /* _M_ALPHA */
    fprintf(stderr, "BREAK: at file %s\n",aFile, aLine);  fflush(stderr);
#endif
#elif defined(XP_UNIX) && !defined(UNIX_CRASH_ON_ASSERT)
    fprintf(stderr, "\07");

    char *assertBehavior = getenv("XPCOM_DEBUG_BREAK");

    if (!assertBehavior) {

      // the default; nothing else to do
      ;

    } else if ( strcmp(assertBehavior, "suspend")== 0 ) {

      // the suspend case is first because we wanna send the signal before 
      // other threads have had a chance to get too far from the state that
      // caused this assertion (in case they happen to have been involved).
      //
      fprintf(stderr, "Suspending process; attach with the debugger.\n");
      kill(0, SIGSTOP);

    } else if ( strcmp(assertBehavior, "warn")==0 ) {
      
      // same as default; nothing else to do (see "suspend" case comment for
      // why this compare isn't done as part of the default case)
      //
      ;

    } else if ( strcmp(assertBehavior,"stack")==0 ) {

      // walk the stack
      //
      nsTraceRefcnt::WalkTheStack(stderr);

    } else if ( strcmp(assertBehavior,"abort")==0 ) {

      // same as UNIX_CRASH_ON_ASSERT
      //
      Abort(aFile, aLine);

    } else {

      fprintf(stderr, "unrecognized value of XPCOM_DEBUG_BREAK env var!\n");

    }    
    fflush(stderr); // this shouldn't really be necessary, i don't think,
                    // but maybe there's some lame stdio that buffers stderr

#elif defined(XP_BEOS)
  {
#ifdef UNIX_CRASH_ON_ASSERT
	char buf[2000];
	sprintf(buf, "BREAK: at file %s, line %d", aFile, aLine);
	DEBUGGER(buf);
#endif
  }
#else
  Abort(aFile, aLine);
#endif
}

NS_COM void nsDebug::Warning(const char* aMessage,
                             const char* aFile, PRIntn aLine)
{
  WARN_PRINTF("%s, file %s, line %d", aMessage, aFile, aLine);
  WARN_FLUSH();
}

//**************** All Dead Code Below
//  Don't use these!
NS_COM void nsDebug::AbortIfFalse(const char* aStr, const char* aExpr,
                                  const char* aFile, PRIntn aLine)
{
   Assertion(aStr, aExpr, aFile, aLine);
}

NS_COM PRBool nsDebug::WarnIfFalse(const char* aStr, const char* aExpr,
                                   const char* aFile, PRIntn aLine)
{
   Assertion(aStr, aExpr, aFile, aLine);
   return PR_TRUE;
}

NS_COM void nsDebug::Abort(const char* aFile, PRIntn aLine)
{
  ABORT_PRINTF("at file %s, line %d", aFile, aLine);
  ABORT_FLUSH();
#if defined(_WIN32)
#ifdef _M_IX86
  long* __p = (long*) 0x7;
  *__p = 0x7;
#else /* _M_ALPHA */
  fprintf(stderr, "\07 Abort\n");  fflush(stderr);
  PR_Abort();
#endif
#elif defined(XP_MAC)
  dprintf("ABORT: at file %s, line %d", aFile, aLine);
  ExitToShell();
#elif defined(XP_UNIX)
  PR_Abort();
#elif defined(XP_OS2)
  DebugBreak();
  return;
#elif defined(XP_BEOS)
  {
#ifndef DEBUG_cls
	char buf[2000];
	sprintf(buf, "ABORT: at file %s, line %d", aFile, aLine);
	DEBUGGER(buf);
#endif
  } 
#endif
}


NS_COM void nsDebug::PreCondition(const char* aStr, const char* aExpr,
                                  const char* aFile, PRIntn aLine)
{
   Assertion(aStr, aExpr, aFile, aLine);
}

NS_COM void nsDebug::PostCondition(const char* aStr, const char* aExpr,
                                   const char* aFile, PRIntn aLine)
{
   Assertion(aStr, aExpr, aFile, aLine);
}

NS_COM void nsDebug::NotYetImplemented(const char* aMessage,
                                       const char* aFile, PRIntn aLine)
{
   Assertion(aMessage, "NotYetImplemented", aFile, aLine);
}

NS_COM void nsDebug::NotReached(const char* aMessage,
                                const char* aFile, PRIntn aLine)
{
   Assertion(aMessage, "Not Reached", aFile, aLine);
}

NS_COM void nsDebug::Error(const char* aMessage,
                           const char* aFile, PRIntn aLine)
{
   Assertion(aMessage, "Error", aFile, aLine);
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_ErrorAccordingToNSPR()
{
    PRErrorCode err = PR_GetError();
    switch (err) {
      case PR_OUT_OF_MEMORY_ERROR:              return NS_ERROR_OUT_OF_MEMORY;
      case PR_WOULD_BLOCK_ERROR:                return NS_BASE_STREAM_WOULD_BLOCK;
      case PR_FILE_NOT_FOUND_ERROR:             return NS_ERROR_FILE_NOT_FOUND;
      case PR_READ_ONLY_FILESYSTEM_ERROR:       return NS_ERROR_FILE_READ_ONLY;
      case PR_NOT_DIRECTORY_ERROR:              return NS_ERROR_FILE_NOT_DIRECTORY;
      case PR_IS_DIRECTORY_ERROR:               return NS_ERROR_FILE_IS_DIRECTORY;
      case PR_LOOP_ERROR:                       return NS_ERROR_FILE_UNRESOLVABLE_SYMLINK;
      case PR_FILE_EXISTS_ERROR:                return NS_ERROR_FILE_ALREADY_EXISTS;
      case PR_FILE_IS_LOCKED_ERROR:             return NS_ERROR_FILE_IS_LOCKED;
      case PR_FILE_TOO_BIG_ERROR:               return NS_ERROR_FILE_TOO_BIG;
      case PR_NO_DEVICE_SPACE_ERROR:            return NS_ERROR_FILE_NO_DEVICE_SPACE;
      case PR_NAME_TOO_LONG_ERROR:              return NS_ERROR_FILE_NAME_TOO_LONG;
      case PR_DIRECTORY_NOT_EMPTY_ERROR:        return NS_ERROR_FILE_DIR_NOT_EMPTY;
      case PR_NO_ACCESS_RIGHTS_ERROR:           return NS_ERROR_FILE_ACCESS_DENIED;
      default:                                  return NS_ERROR_FAILURE;
    }
}

////////////////////////////////////////////////////////////////////////////////
// This wrapper around PR_CurrentThread is simply here for debug builds so
// that clients linking with xpcom don't also have to link with nspr explicitly.

#if defined(NS_DEBUG) && defined(NS_MT_SUPPORTED)

#include "nsISupportsUtils.h"
#include "prthread.h"

extern "C" NS_EXPORT void* NS_CurrentThread(void);
extern "C" NS_EXPORT void NS_CheckThreadSafe(void* owningThread, const char* msg);

void*
NS_CurrentThread(void)
{
  void* th = PR_CurrentThread();
  return th;
}

/*
 * DON'T TOUCH THAT DIAL...
 * For now, we're making the thread-safety checking be on by default which, yes, slows
 * down linux a bit... but we're doing it so that everybody has a chance to exercise 
 * their code a good deal before the beta. After we branch, we'll turn this back off
 * and then you can enable it explicitly by setting XPCOM_CHECK_THREADSAFE. This will
 * let you verify the thread-safety of your classes without impacting everyone all the
 * time.
 */
static PRBool gCheckThreadSafeDefault = PR_TRUE; // READ THE ABOVE COMMENT FIRST!

void
NS_CheckThreadSafe(void* owningThread, const char* msg)
{
  static int check = -1;
  if (check == -1) {
    const char *eVar = getenv("XPCOM_CHECK_THREADSAFE");
    if (eVar && *eVar == '0')
	check = 0;
    else
        check = gCheckThreadSafeDefault || eVar != 0;
  }
  if (check) {
    NS_ASSERTION(owningThread == NS_CurrentThread(), msg);
  }
}

#endif // !(defined(NS_DEBUG) && defined(NS_MT_SUPPORTED))

////////////////////////////////////////////////////////////////////////////////
