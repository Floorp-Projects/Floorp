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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
 *   Henry Sobotka
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

#include "nsDebugImpl.h"
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
#endif

#if defined(XP_UNIX) || defined(_WIN32) || defined(XP_OS2) || defined(XP_BEOS)
/* for abort() and getenv() */
#include <stdlib.h>
#endif

#if defined(XP_UNIX) && !defined(UNIX_CRASH_ON_ASSERT)
#include <signal.h>
/* for nsTraceRefcnt::WalkTheStack() */
#include "nsISupportsUtils.h"
#include "nsTraceRefcntImpl.h"

#if defined(__GNUC__) && defined(__i386)
#  define DebugBreak() { asm("int $3"); }
#elif defined(__APPLE__)
#  include "MacTypes.h"
#  define DebugBreak() { Debugger(); }
#else
#  define DebugBreak()
#endif
#endif

#if defined(XP_OS2)
  /* Added definitions for DebugBreak() for 2 different OS/2 compilers.  Doing
   * the int3 on purpose so that a developer can step over the
   * instruction if so desired.  Not always possible if trapping due to exception
   * handling IBM-AKR
   */
  #define INCL_WINDIALOGS  // need for WinMessageBox
  #include <os2.h>
  #include <string.h>
  
  #if defined(DEBUG)
    #define DebugBreak() { asm("int $3"); }
  #else
    #define DebugBreak()
  #endif /* DEBUG */
#endif /* XP_OS2 */

#if defined(_WIN32)
#include <windows.h>
#include <signal.h>
#elif defined(XP_MAC)
   #define TEMP_MAC_HACK
   
   //------------------------
   #ifdef TEMP_MAC_HACK
	   #include <MacTypes.h>
	   #include <Processes.h>
	   #include <string.h>

	   // TEMPORARY UNTIL WE HAVE MACINTOSH ENVIRONMENT VARIABLES THAT CAN TURN ON
	   // LOGGING ON MACINTOSH
	   // At this moment, NSPR's logging is a no-op on Macintosh.

	   #include <stdarg.h>
	   #include <stdio.h>
	 
	   #undef PR_LOG
	   #undef PR_LogFlush
	   #define PR_LOG(module,level,args) dprintf args
	   #define PR_LogFlush()
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
   #endif // TEMP_MAC_HACK
   //------------------------
#elif defined(XP_UNIX)
#include<stdlib.h>
#endif

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

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDebugImpl, nsIDebug)

nsDebugImpl::nsDebugImpl()
{
}

/**
 * Implementation of the nsDebug methods. Note that this code is
 * always compiled in, in case some other module that uses it is
 * compiled with debugging even if this library is not.
 */
static PRLogModuleInfo* gDebugLog;

static void InitLog(void)
{
  if (0 == gDebugLog) {
    gDebugLog = PR_NewLogModule("nsDebug");
    gDebugLog->level = PR_LOG_DEBUG;
  }
}



NS_IMETHODIMP
nsDebugImpl::Assertion(const char *aStr, const char *aExpr, const char *aFile, PRInt32 aLine)
{
   InitLog();

   char buf[1000];
   PR_snprintf(buf, sizeof(buf),
              "###!!! ASSERTION: %s: '%s', file %s, line %d",
              aStr, aExpr, aFile, aLine);

   // Write out the assertion message to the debug log
   PR_LOG(gDebugLog, PR_LOG_ERROR, ("%s", buf));
   PR_LogFlush();

   // And write it out to the stderr
   fprintf(stderr, "%s\n", buf);
   fflush(stderr);

#if defined(_WIN32)
   char* assertBehavior = getenv("XPCOM_DEBUG_BREAK");
   if (assertBehavior && strcmp(assertBehavior, "warn") == 0)
     return NS_OK;

   if(!InDebugger())
      {
      DWORD code = IDRETRY;

      /* Create the debug dialog out of process to avoid the crashes caused by 
       * Windows events leaking into our event loop from an in process dialog.
       * We do this by launching windbgdlg.exe (built in xpcom/windbgdlg).
       * See http://bugzilla.mozilla.org/show_bug.cgi?id=54792
       */
      PROCESS_INFORMATION pi;
      STARTUPINFO si;
      char executable[MAX_PATH];
      char* pName;

      memset(&pi, 0, sizeof(pi));

      memset(&si, 0, sizeof(si));
      si.cb          = sizeof(si);
      si.wShowWindow = SW_SHOW;

      if(GetModuleFileName(GetModuleHandle("xpcom.dll"), executable, MAX_PATH) &&
         NULL != (pName = strrchr(executable, '\\')) &&
         NULL != strcpy(pName+1, "windbgdlg.exe") &&
#ifdef DEBUG_jband
         (printf("Launching %s\n", executable), PR_TRUE) &&
#endif         
         CreateProcess(executable, buf, NULL, NULL, PR_FALSE,
                       DETACHED_PROCESS | NORMAL_PRIORITY_CLASS,
                       NULL, NULL, &si, &pi) &&
         WAIT_OBJECT_0 == WaitForSingleObject(pi.hProcess, INFINITE) &&
         GetExitCodeProcess(pi.hProcess, &code))
      {
        CloseHandle(pi.hProcess);
      }                         

      switch(code)
         {
         case IDABORT:
            //This should exit us
            raise(SIGABRT);
            //If we are ignored exit this way..
            _exit(3);
            break;
         
         case IDIGNORE:
            return NS_OK;
            // Fall Through
         }
      }
#endif

#if defined(XP_OS2)
   char* assertBehavior = getenv("XPCOM_DEBUG_BREAK");
   if (assertBehavior && strcmp(assertBehavior, "warn") == 0)
     return NS_OK;

      char msg[1200];
      PR_snprintf(msg, sizeof(msg),
                "%s\n\nClick Cancel to Debug Application.\n"
                "Click Enter to continue running the Application.", buf);
      ULONG code = MBID_ERROR;
      code = WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, msg, 
                           "nsDebug::Assertion", 0,
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
         return NS_OK;
         // If Retry, Fall Through
      }
#endif

      Break(aFile, aLine);
      return NS_OK;
}

NS_IMETHODIMP 
nsDebugImpl::Break(const char *aFile, PRInt32 aLine)
{
#ifndef TEMP_MAC_HACK
  // Write out the assertion message to the debug log
   InitLog();

   PR_LOG(gDebugLog, PR_LOG_ERROR, 
         ("###!!! Break: at file %s, line %d", aFile, aLine));
   PR_LogFlush();

  fprintf(stderr, "Break: at file %s, line %d\n",aFile, aLine);  fflush(stderr);
  fflush(stderr);

#if defined(_WIN32)
#ifdef _M_IX86
   ::DebugBreak();
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

    } 
    else if ( strcmp(assertBehavior,"stack")==0 ) {

      // walk the stack
      //
      nsTraceRefcntImpl::WalkTheStack(stderr);
    } 
    else if ( strcmp(assertBehavior,"abort")==0 ) {

      // same as UNIX_CRASH_ON_ASSERT
      //
      Abort(aFile, aLine);

    } else if ( strcmp(assertBehavior,"trap")==0 ) {

      DebugBreak();

    } else {

      fprintf(stderr, "unrecognized value of XPCOM_DEBUG_BREAK env var!\n");

    }    
    fflush(stderr); // this shouldn't really be necessary, i don't think,
                    // but maybe there's some lame stdio that buffers stderr

#elif defined(XP_BEOS)
  {
#ifdef UNIX_CRASH_ON_ASSERT
	char buf[2000];
	sprintf(buf, "Break: at file %s, line %d", aFile, aLine);
	DEBUGGER(buf);
#endif
  }
#else
  Abort(aFile, aLine);
#endif
#endif // TEMP_MAC_HACK
  return NS_OK;
}

NS_IMETHODIMP 
nsDebugImpl::Abort(const char *aFile, PRInt32 aLine)
{
  InitLog();

   PR_LOG(gDebugLog, PR_LOG_ERROR, 
         ("###!!! Abort: at file %s, line %d", aFile, aLine));
   PR_LogFlush();
   fprintf(stderr, "\07 Abort\n");  fflush(stderr);
   fflush(stderr);

#if defined(_WIN32)
#ifdef _M_IX86
  long* __p = (long*) 0x7;
  *__p = 0x7;
#else /* _M_ALPHA */
  PR_Abort();
#endif
#elif defined(XP_MAC)
  ExitToShell();
#elif defined(XP_UNIX)
  PR_Abort();
#elif defined(XP_OS2)
  DebugBreak();
  return NS_OK;
#elif defined(XP_BEOS)
  {
#ifndef DEBUG_cls
	char buf[2000];
	sprintf(buf, "Abort: at file %s, line %d", aFile, aLine);
	DEBUGGER(buf);
#endif
  } 
#endif
  return NS_OK;
}

NS_IMETHODIMP 
nsDebugImpl::Warning(const char* aMessage,
                     const char* aFile, PRIntn aLine)
{
  InitLog();

  char buf[1000];
  PR_snprintf(buf, sizeof(buf),
              "WARNING: %s, file %s, line %d",
              aMessage, aFile, aLine);

  // Write out the warning message to the debug log
  PR_LOG(gDebugLog, PR_LOG_ERROR, ("%s", buf));

  // And write it out to the stdout
  fprintf(stderr, "%s\n", buf);
  fflush(stderr);
  return NS_OK;
}

NS_METHOD
nsDebugImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
  *aInstancePtr = nsnull;
  nsIDebug* debug = new nsDebugImpl();
  if (!debug)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult rv = debug->QueryInterface(aIID, aInstancePtr);
  if (NS_FAILED(rv)) {
    delete debug;
  }
  
  return rv;
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

