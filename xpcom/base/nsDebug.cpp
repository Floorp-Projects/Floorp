/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

#include "nsDebug.h"
#include "prlog.h"
#include "prinit.h"

#if defined(XP_BEOS)
/* For DEBUGGER macros */
#include <Debug.h>
#endif

#if defined(XP_UNIX)
/* for abort() */
#include <stdlib.h>
#endif

#if defined(_WIN32)
#include <windows.h>
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
	      buffer[0] = vsnprintf((char *)buffer + 1, sizeof(buffer) - 1, format, ap);
	      va_end(ap);
	      if (strstr(format, "Warning: ") == format)
	 	      printf("еее%s\n", (char*)buffer + 1);
	 	  else
	 	      DebugStr(buffer);
	   }
   #endif // TEMP_MAC_HACK
   //------------------------
#elif defined(XP_UNIX)
#include<stdlib.h>
#endif

/**
 * Define output so users will always see it
 */

#if defined(XP_UNIX) || (defined(_M_ALPHA) && defined(_WIN32))
#define DBG_LOG(log,err,pargs) \
  InitLog(); \
  PR_LOG(log,err,pargs); \
  PR_LogFlush(); \
  printf pargs; putchar('\n');
#else
#define DBG_LOG(log,err,pargs) \
  InitLog(); \
  PR_LOG(log,err,pargs); \
  PR_LogFlush();
#endif

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

NS_COM void nsDebug::Abort(const char* aFile, PRIntn aLine)
{
  DBG_LOG(gDebugLog, PR_LOG_ERROR,
          ("Abort: at file %s, line %d", aFile, aLine));
#if defined(_WIN32)
#ifdef _M_IX86
  long* __p = (long*) 0x7;
  *__p = 0x7;
#else /* _M_ALPHA */
  fprintf(stderr, "\07 Abort\n");  fflush(stderr);
  PR_Abort();
#endif
#elif defined(XP_MAC)
  ExitToShell();
#elif defined(XP_UNIX)
  PR_Abort();
#elif defined(XP_BEOS)
  {
	char buf[2000];
	sprintf(buf, "Abort: at file %s, line %d", aFile, aLine);
	DEBUGGER(buf);
  } 
#endif
}

NS_COM void nsDebug::Break(const char* aFile, PRIntn aLine)
{
#ifndef TEMP_MAC_HACK
  DBG_LOG(gDebugLog, PR_LOG_ERROR,
          ("Break: at file %s, line %d", aFile, aLine));
#if defined(_WIN32)
#ifdef _M_IX86
   ::DebugBreak();
#else /* _M_ALPHA */
   fprintf(stderr, "Break: at file %s\n",aFile, aLine);  fflush(stderr);
#endif
#elif defined(XP_UNIX) && !defined(UNIX_CRASH_ON_ASSERT)
  fprintf(stderr, "\07");  fflush(stderr);
#elif defined(XP_BEOS)
  {
	char buf[2000];
	sprintf(buf, "Break: at file %s, line %d", aFile, aLine);
	DEBUGGER(buf);
  }
#else
  Abort(aFile, aLine);
#endif
#endif // TEMP_MAC_HACK
}

NS_COM void nsDebug::PreCondition(const char* aStr, const char* aExpr,
                                  const char* aFile, PRIntn aLine)
{
  DBG_LOG(gDebugLog, PR_LOG_ERROR,
          ("PreCondition: \"%s\" (%s) at file %s, line %d", aStr, aExpr,
           aFile, aLine));
  Break(aFile, aLine);
}

NS_COM void nsDebug::PostCondition(const char* aStr, const char* aExpr,
                                   const char* aFile, PRIntn aLine)
{
  DBG_LOG(gDebugLog, PR_LOG_ERROR,
          ("PostCondition: \"%s\" (%s) at file %s, line %d", aStr, aExpr,
           aFile, aLine));
  Break(aFile, aLine);
}

NS_COM void nsDebug::Assertion(const char* aStr, const char* aExpr,
                               const char* aFile, PRIntn aLine)
{
  DBG_LOG(gDebugLog, PR_LOG_ERROR,
          ("Assertion: \"%s\" (%s) at file %s, line %d", aStr, aExpr,
           aFile, aLine));
  Break(aFile, aLine);
}

NS_COM void nsDebug::NotYetImplemented(const char* aMessage,
                                       const char* aFile, PRIntn aLine)
{
  DBG_LOG(gDebugLog, PR_LOG_ERROR,
          ("NotYetImplemented: \"%s\" at file %s, line %d", aMessage,
           aFile, aLine));
  Break(aFile, aLine);
}

NS_COM void nsDebug::NotReached(const char* aMessage,
                                const char* aFile, PRIntn aLine)
{
  DBG_LOG(gDebugLog, PR_LOG_ERROR,
          ("NotReached: \"%s\" at file %s, line %d", aMessage, aFile, aLine));
  Break(aFile, aLine);
}

NS_COM void nsDebug::Error(const char* aMessage,
                           const char* aFile, PRIntn aLine)
{
  DBG_LOG(gDebugLog, PR_LOG_ERROR,
          ("Error: \"%s\" at file %s, line %d", aMessage, aFile, aLine));
  Break(aFile, aLine);
}

NS_COM void nsDebug::Warning(const char* aMessage,
                             const char* aFile, PRIntn aLine)
{
  DBG_LOG(gDebugLog, PR_LOG_ERROR,
          ("Warning: \"%s\" at file %s, line %d", aMessage, aFile, aLine));
}
