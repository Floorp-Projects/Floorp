/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nslog_h__
#define nslog_h__

/**
 * LOGGING SERVICE
 *
 * We all know and love PR_LOG, but let's face it, it has some deficiencies:
 *      - can't direct output for different logs to different places,
 *      - isn't scriptable,
 *      - can't control printing format, including indentation,
 *      - it's ugly.
 * We've solved these problems with xpcom logging facility, nslog, the new modern
 * equivalent. Here's how you use it:
 *
 *     #include "nslog.h"
 *
 *     // First declare a new log:
 *     NS_IMPL_LOG(Foo)
 *
 *     // Then define PRINTF and FLUSH routines for your log module:
 *     #define PRINTF NS_LOG_PRINT(Foo)
 *     #define FLUSH  NS_LOG_FLUSH(Foo)
 *
 *     void main() {
 *         // Then use it like this:
 *         PRINTF("hello world %d", 42);
 *
 *         FLUSH(); // not necessary, but might be useful for synchronization
 *     }
 *
 * The above log statement will print something like this:
 *
 *     a33e50 Foo         hello world 42
 *
 * The hex number at the beginning of the line indicates the ID of the thread
 * executing the PRINTF statement. The name of the log appears next.
 *
 * A cool feature of how these macros work is that when NS_DISABLE_LOGGING is
 * defined, all the logging code macro-expands to nothing. 
 *
 * You can also cause it to indent its output to help you log recursive execution:
 *
 *     int fact(int n) {
 *        NS_LOG_WITH_INDENT(Foo, "fact");
 *        PRINTF("calling fact of %d\n", n);
 *        if (n == 0) return 1;
 *        int result = n * fact(n - 1);
 *        PRINTF("fact of %d is %d\n", n, result);
 *        return result;
 *     }
 *
 * Calling fact(3) will produce a log that looks like this:
 *
 *     8e28b0 Foo      [ Begin fact
 *     8e28b0 Foo      |  calling fact of 3
 *     8e28b0 Foo      |  [ Begin fact
 *     8e28b0 Foo      |  |  calling fact of 2
 *     8e28b0 Foo      |  |  [ Begin fact
 *     8e28b0 Foo      |  |  |  calling fact of 1
 *     8e28b0 Foo      |  |  |  [ Begin fact
 *     8e28b0 Foo      |  |  |  |  calling fact of 0
 *     8e28b0 Foo      |  |  |  ] End   fact
 *     8e28b0 Foo      |  |  |  fact of 1 is 1
 *     8e28b0 Foo      |  |  ] End   fact
 *     8e28b0 Foo      |  |  fact of 2 is 2
 *     8e28b0 Foo      |  ] End   fact
 *     8e28b0 Foo      |  fact of 3 is 6
 *     8e28b0 Foo      ] End   fact
 *
 * If the second argument to NS_LOG_WITH_INDENT is nsnull, you don't get the 
 * Begin/End statements:
 *
 *     8e28b0 Foo      |  calling fact of 3
 *     8e28b0 Foo      |  |  calling fact of 2
 *     8e28b0 Foo      |  |  |  calling fact of 1
 *     8e28b0 Foo      |  |  |  |  calling fact of 0
 *     8e28b0 Foo      |  |  |  fact of 1 is 1
 *     8e28b0 Foo      |  |  fact of 2 is 2
 *     8e28b0 Foo      |  fact of 3 is 6
 *
 * You can control where the output of the log goes by setting its
 * log event sink to an appropriate nsILogEventSink. By default the
 * file log event sink does the following:
 *      - outputs to stderr
 *      - outputs to the platform's debug output for errors
 * New log event sinks can be instantiated via the component manager:
 *
 *     #ifdef NS_ENABLE_LOGGING
 *         nsComponentManager::CreateInstance(kFileLogEventSinkCID,
 *                                            nsnull,
 *                                            NS_GET_IID(nsIFileLogEventSink),
 *                                            &logEventSink);
 *         logEventSink->InitFromFILE("stdout", stdout);
 *         Foo->SetLogEventSink(logEventSink);
 *     #endif
 *
 * or the application can implement its own nsILogEventSink.
 */

#include "prlog.h" // include prlog.h because we're going to override it's ancient macros
#include "nsDebug.h"
#include "nsILoggingService.h"
#include "nsCOMPtr.h"

#if (defined(DEBUG) || defined(NS_DEBUG) || defined(FORCE_PR_LOG)) && !defined(WIN16)
// enable logging by default
#define NS_ENABLE_LOGGING
#endif

#ifdef NS_DISABLE_LOGGING
// override, if you want DEBUG, but *not* logging (for some reason)
#undef NS_ENABLE_LOGGING
#endif

#ifdef NS_ENABLE_LOGGING

////////////////////////////////////////////////////////////////////////////////

PR_EXTERN(nsILog*)
NS_GetLog(const char* name, PRUint32 controlFlags = 0);

////////////////////////////////////////////////////////////////////////////////

/**
 * nsLogIndent: This class allows indentation to occur in a block scope. The
 * automatic destructor takes care of resetting the indentation. Use the
 * NS_LOG_WITH_INDENT macro.
 */
class NS_COM nsLogIndent
{
public:
    nsLogIndent(nsILog* log, const char* msg);
    ~nsLogIndent();

protected:
    nsILog*     mLog;
    const char* mHeaderMsg;
};

////////////////////////////////////////////////////////////////////////////////
// macros

#define NS_DECL_LOG(_log)                                     \
    extern nsILog* _log;

#define NS_IMPL_LOG(_log)                                     \
    nsILog* _log = NS_GetLog(#_log);

#define NS_IMPL_LOG_ENABLED(_log)                             \
    nsILog* _log = NS_GetLog(#_log, nsILog::DEFAULT_ENABLED);

#define NS_LOG_ENABLED(_log)                                  \
    ((_log) && (_log)->Test())

#define NS_LOG_PRINTF(_log)                                   \
    (!NS_LOG_ENABLED(_log)) ? NS_OK : (_log)->Printf

#define NS_LOG_FLUSH(_log)                (_log)->Flush

#define NS_LOG_WITH_INDENT(_log, _msg)    nsLogIndent _indent_##_log(_log, _msg)
#define NS_LOG_BEGIN_INDENT(_log)         ((_log)->IncreaseIndent())
#define NS_LOG_END_INDENT(_log)           ((_log)->DecreaseIndent())

#else  // !NS_ENABLE_LOGGING

inline void nsNoop(...) {}

#define NS_DECL_LOG(_log)                 /* nothing */
#define NS_IMPL_LOG(_log)                 /* nothing */
#define NS_IMPL_LOG_ENABLED(_log)         /* nothing */
#define NS_LOG_ENABLED(_log)              0
#define NS_LOG_PRINTF(_log)               nsNoop
#define NS_LOG_FLUSH(_log)                nsNoop
#define NS_LOG_WITH_INDENT(_log, _msg)    ((void)0)
#define NS_LOG_BEGIN_INDENT(_log)         ((void)0)
#define NS_LOG_END_INDENT(_log)           ((void)0)

#endif // !NS_ENABLE_LOGGING

// Redefine NSPR's logging system:
#undef  PR_LOG_TEST
#define PR_LOG_TEST(_log, _level)         NS_LOG_ENABLED(_log)
#undef  PR_LOG
#define PR_LOG(_log, _level, _args)       NS_LOG_PRINTF(_log) _args
#define PRLogModuleInfo                   #error use_NS_DECL_LOG_instead
#define PR_NewLogModule                   #error use_NS_IMPL_LOG_instead
#undef  PR_ASSERT
#define PR_ASSERT(x)                      NS_ASSERTION(x, #x)
#undef printf
#define printf                            use_NS_LOG_PRINTF_instead
#undef fprintf
#define fprintf                           use_NS_LOG_PRINTF_instead

////////////////////////////////////////////////////////////////////////////////
#endif // nslog_h__
