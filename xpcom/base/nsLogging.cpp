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

#include "nscore.h"
#include "nsLogging.h"

#ifdef NS_ENABLE_LOGGING

#include "nsCRT.h"
#include "prthread.h"
#include "nsAutoLock.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsSpecialSystemDirectory.h"
#ifdef XP_WIN
#include <windows.h>
#endif

#undef printf
#undef fprintf

static PRMonitor* gLogMonitor = nsnull;
static nsObjectHashtable* gSettings = nsnull;

NS_IMPL_LOG_ENABLED(LogInfo)
#define PRINTF  NS_LOG_PRINTF(LogInfo)
#define FLUSH   NS_LOG_FLUSH(LogInfo)

NS_DEFINE_CID(kLoggingServiceCID, NS_LOGGINGSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////
// nsLoggingService

static nsLoggingService* gLoggingService = nsnull;

nsLoggingService::nsLoggingService()
    : mLogs(16),
      mDefaultControlFlags(nsILog::DEFAULT_DISABLED |
                           nsILog::PRINT_THREAD_ID |
                           nsILog::PRINT_LOG_NAME)
{
    NS_INIT_ISUPPORTS();
}

nsLoggingService::~nsLoggingService()
{
#ifdef DEBUG_warren
    DescribeLogs(LogInfo);
#endif
}

NS_IMPL_QUERY_INTERFACE1(nsLoggingService, nsILoggingService)
NS_IMPL_ADDREF(nsLoggingService)

NS_IMETHODIMP_(nsrefcnt) 
nsLoggingService::Release(void) 
{
  NS_PRECONDITION(0 != mRefCnt, "dup release"); 
  NS_ASSERT_OWNINGTHREAD(nsLoggingService); 
  --mRefCnt; 
  NS_LOG_RELEASE(this, mRefCnt, "nsLoggingService"); 
  if (mRefCnt == 0) {
    mRefCnt = 1; /* stabilize */ 
    NS_DELETEXPCOM(this);
    
    // special action -- null out global 
    gLoggingService = nsnull;

    return 0; 
  } 
  return mRefCnt; 
} 

static void* PR_CALLBACK
levelClone(nsHashKey *aKey, void *aData, void* closure)
{
    PRUint32 level = NS_PTR_TO_INT32(aData);
    return (void*)level;
}

static PRBool PR_CALLBACK
levelDestroy(nsHashKey *aKey, void *aData, void* closure)
{
    return PR_TRUE;
}

static void
RecordSetting(const char* name, const char* value)
{
    PRUint32 level = 2;
    if (nsCRT::strcasecmp(value, "ERROR") == 0 ||
        nsCRT::strcasecmp(value, "2") == 0) {
        level = 2;
        PRINTF("### NS_LOG: %s = ERROR\n", name);
    }
    else if (nsCRT::strcasecmp(value, "WARN") == 0 ||
             nsCRT::strcasecmp(value, "WARNING") == 0 ||
             nsCRT::strcasecmp(value, "3") == 0) {
        level = 3;
        PRINTF("### NS_LOG: %s = WARN\n", name);
    }
    else if (nsCRT::strcasecmp(value, "STDOUT") == 0 ||
             nsCRT::strcasecmp(value, "OUT") == 0 ||
             nsCRT::strcasecmp(value, "4") == 0) {
        level = 4;
        PRINTF("### NS_LOG: %s = STDOUT\n", name);
    }
    else if (nsCRT::strcasecmp(value, "DBG") == 0 ||
             nsCRT::strcasecmp(value, "DEBUG") == 0 ||
             nsCRT::strcasecmp(value, "5") == 0) {
        level = 5;
        PRINTF("### NS_LOG: %s = DBG\n", name);
    }
    else {
        PRINTF("### NS_LOG error: %s = %s (bad level)\n", name, value);
    }

    nsCStringKey key(name);
    gSettings->Put(&key, (void*)level);
}

nsresult
nsLoggingService::Init()
{
    nsresult rv;
    nsFileLogEventSink* defaultSink = nsnull;
    const char* outputPath = nsnull;

    if (gLogMonitor == nsnull) {
        gLogMonitor = PR_NewMonitor();
        if (gLogMonitor == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    if (gSettings == nsnull) {
        gSettings = new nsObjectHashtable(levelClone, nsnull,
                                          levelDestroy, nsnull,
                                          16);
        if (gSettings == nsnull) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto error;
        }
    }

    // try the nspr log environment variables first:
    {
        const char* nspr_log_modules = getenv("NSPR_LOG_MODULES");
        if (nspr_log_modules) {
            PRINTF("### NS_LOG: using NSPR_LOG_MODULES (instead of .nslog)\n");
            char* head = nsCRT::strdup(nspr_log_modules);
            char* rest = nsCRT::strdup(nspr_log_modules);
            while (1) {
                char* name = nsCRT::strtok(rest, ":", &rest);
                if (name == nsnull) break;
                char* value = nsCRT::strtok(rest, ";", &rest);
                if (value == nsnull) break;
                RecordSetting(name, value);
            }
            nsCRT::free(head);
        }
        const char* nspr_log_file = getenv("NSPR_LOG_FILE");
        if (nspr_log_file) {
            PRINTF("### NS_LOG: using NSPR_LOG_FILE (instead of .nslog) -- logging to %s\n", 
                    nspr_log_file);
            outputPath = nspr_log_file;
        }
    }
    // then load up log description file:
    {
        nsSpecialSystemDirectory file(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
        file += ".nslog";
        const char* path = file.GetNativePathCString();
        FILE* f = ::fopen(path, "r");
        if (f != nsnull) {
            PRInt32 cnt;
            while (PR_TRUE) {
                char name[64];
                char value[64];
//                cnt = ::fscanf(f, "%64s=%64s\n", name, value);
                cnt = ::fscanf(f, "%64s\n", name);
                if (cnt <= 0) break;
                cnt = ::fscanf(f, "%64s\n", value);
                if (cnt <= 0) break;
                RecordSetting(name, value);
            }
            ::fclose(f);
        }
    }

    defaultSink = new nsFileLogEventSink();
    if (defaultSink == nsnull) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto error;
    }

    NS_ADDREF(defaultSink);
    if (outputPath)
        rv = defaultSink->Init(outputPath);
    else
        rv = defaultSink->InitFromFILE("stderr", stderr);
    if (NS_FAILED(rv)) goto error;

    mDefaultSink = defaultSink;
    NS_RELEASE(defaultSink);

#ifdef DEBUG
    DescribeLogs(LogInfo);
#endif
    return NS_OK;

  error:
    NS_IF_RELEASE(defaultSink);
    if (gLogMonitor) {
        PR_DestroyMonitor(gLogMonitor);
        gLogMonitor = nsnull;
    }
    return rv;
}

static nsresult
EnsureLoggingService()
{
    nsresult rv;
    if (gLoggingService == nsnull) {
        gLoggingService = new nsLoggingService();
        if (gLoggingService == NULL)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = gLoggingService->Init();
        if (NS_FAILED(rv)) {
            delete gLoggingService;
            gLoggingService = nsnull;
            return rv; 
        }
        // Note that there's no AddRef here. That's because when the service manager
        // gets around to calling Create (below) sometime later, we'll AddRef it then
        // and the service manager will be the sole owner. This allows us to use it
        // before xpcom has started up.
    }
    return NS_OK;
}

NS_METHOD
nsLoggingService::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    nsresult rv;
    if (outer)
        return NS_ERROR_NO_AGGREGATION;

    if (gLoggingService == nsnull) {
        rv = EnsureLoggingService();
        if (NS_FAILED(rv)) return rv;
    }

    return gLoggingService->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsLoggingService::GetLog(const char* name, nsILog* *result)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    nsCStringKey key(name);
    nsILog* log = (nsILog*)mLogs.Get(&key);
    if (log) {
        *result = log;
        return NS_OK;
    }

    nsLog* newLog = new nsLog();
    if (newLog == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = newLog->Init(name, mDefaultControlFlags, mDefaultSink);
    if (NS_FAILED(rv)) {
        delete newLog;
        return rv;
    }
    NS_ADDREF(newLog);
    mLogs.Put(&key, newLog);
    *result = newLog;
    return NS_OK;
}

PR_IMPLEMENT(nsILog*)
NS_GetLog(const char* name, PRUint32 controlFlags)
{
    nsresult rv;

    if (gLoggingService == nsnull) {
        rv = EnsureLoggingService();
        if (NS_FAILED(rv)) return nsnull;
    }
    
    nsILog* log;
    rv = gLoggingService->GetLog(name, &log);
    if (NS_FAILED(rv)) return nsnull;

    // add in additional flags:
    PRUint32 flags;
    log->GetControlFlags(&flags);
    flags |= controlFlags;
    log->SetControlFlags(flags);

    return log;
}

static PRBool PR_CALLBACK
DescribeLog(nsHashKey *aKey, void *aData, void* closure)
{
    nsILog* log = (nsILog*)aData;
    nsILog* out = (nsILog*)closure;
    (void)log->Describe(out);
    return PR_TRUE;
}

NS_IMETHODIMP
nsLoggingService::DescribeLogs(nsILog* out)
{
    NS_LOG_PRINTF(out)("%-20.20s %-8.8s %s\n", "LOG NAME", "ENABLED", "DESTINATION");
    mLogs.Enumerate(DescribeLog, out);
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingService::GetDefaultControlFlags(PRUint32 *controlFlags)
{
    *controlFlags = mDefaultControlFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingService::SetDefaultControlFlags(PRUint32 controlFlags)
{
    mDefaultControlFlags = controlFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingService::GetDefaultLogEventSink(nsILogEventSink* *sink)
{
    *sink = mDefaultSink;
    NS_ADDREF(*sink);
    return NS_OK;
}

NS_IMETHODIMP
nsLoggingService::SetDefaultLogEventSink(nsILogEventSink* sink)
{
    mDefaultSink = sink;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsLog

nsLog::nsLog()
    : mName(nsnull),
      mIndentLevel(0)
{
    NS_INIT_ISUPPORTS();
}

nsLog::~nsLog()
{
    if (mName) nsCRT::free(mName);
}

// !!! We don't use NS_IMPL_ISUPPORTS for nsLog because logs always appear to 
// leak due to the way NS_IMPL_LOG works.

NS_IMETHODIMP_(nsrefcnt)
nsLog::AddRef(void)
{
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");
  NS_ASSERT_OWNINGTHREAD(nsLog);
  ++mRefCnt;
  return mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt)
nsLog::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  NS_ASSERT_OWNINGTHREAD(nsLog);
  --mRefCnt;
  if (mRefCnt == 0) {
    mRefCnt = 1; /* stabilize */
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

NS_IMPL_QUERY_INTERFACE1(nsLog, nsILog)

nsresult
nsLog::Init(const char* name, PRUint32 controlFlags, nsILogEventSink* sink)
{
    mName = nsCRT::strdup(name);
    if (mName == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    mControlFlags = controlFlags; 
    mSink = sink;

    nsCStringKey key(name);
    PRUint32 level = NS_PTR_TO_INT32(gSettings->Get(&key));
    if (level != 0) {
        mControlFlags |= nsILog::DEFAULT_ENABLED;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsLog::GetName(char* *aName)
{
    *aName = nsCRT::strdup(mName);
    return *aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsLog::Enabled(PRBool *result)
{
    *result = Test();
    return NS_OK;
}

NS_IMETHODIMP
nsLog::Print(const PRUnichar *message)
{
    nsAutoMonitor monitor(gLogMonitor);

    nsCString str; str.AssignWithConversion(message);
    nsresult rv = Printf(str.get());
    return rv;
}

NS_IMETHODIMP
nsLog::Flush(void)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    return mSink->Flush(this);
}

NS_IMETHODIMP
nsLog::IncreaseIndent()
{
    nsAutoMonitor monitor(gLogMonitor);
    
    mIndentLevel++;
    return NS_OK;
}

NS_IMETHODIMP
nsLog::DecreaseIndent()
{
    nsAutoMonitor monitor(gLogMonitor);
    
    PR_ASSERT(mIndentLevel > 0);
    if (mIndentLevel == 0)
        return NS_ERROR_FAILURE;
    mIndentLevel--;
    return NS_OK;
}

NS_IMETHODIMP
nsLog::GetIndentLevel(PRUint32 *aIndentLevel)
{
    *aIndentLevel = mIndentLevel;
    return NS_OK;
}

NS_IMETHODIMP
nsLog::Describe(nsILog* out)
{
    nsresult rv;
#if 0
    const char* levelName;
    switch (mEnabledLevel) {
      case nsILog::LEVEL_NEVER:  levelName = "NEVER"; break;
      case nsILog::LEVEL_ERROR:  levelName = "ERROR"; break;
      case nsILog::LEVEL_WARN:   levelName = "WARN"; break;
      case nsILog::LEVEL_STDOUT: levelName = "STDOUT"; break;
      case nsILog::LEVEL_DBG:    levelName = "DBG"; break;
      default:                   levelName = "<unknown>"; break;
    }
#endif
    char* dest = nsnull;
    rv = mSink->GetDestinationName(&dest);
    if (NS_FAILED(rv)) {
        dest = nsCRT::strdup("<unknown>");
    }
//    NS_LOG(out, ("  %-20.20s %-8.8s %s\n", mName, levelName, dest));
    PRBool enabled = mControlFlags & nsILog::DEFAULT_ENABLED;
    NS_LOG_PRINTF(out)("%-20.20s %-8.8s %s\n", 
                       mName, (enabled ? "yes" : "no"), dest);
    if (dest) nsCRT::free(dest);
    return NS_OK;
}

NS_IMETHODIMP
nsLog::GetControlFlags(PRUint32 *flags)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    *flags = mControlFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsLog::SetControlFlags(PRUint32 flags)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    mControlFlags = flags;
    return NS_OK;
}

NS_IMETHODIMP
nsLog::GetLogEventSink(nsILogEventSink* *sink)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    *sink = mSink;
    NS_ADDREF(*sink);
    return NS_OK;
}

NS_IMETHODIMP
nsLog::SetLogEventSink(nsILogEventSink* sink)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    mSink = sink;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsLog

NS_IMETHODIMP
nsLog::Printf(const char* format, ...)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    va_list args;
    va_start(args, format);
    nsresult rv = Vprintf(format, args);
    va_end(args);
    return rv;
}

NS_IMETHODIMP
nsLog::Vprintf(const char* format, va_list args)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    char* msg = PR_vsmprintf(format, args);
    if (!msg) return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = mSink->Print(this, msg);
    PR_smprintf_free(msg);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsLogIndent

nsLogIndent::nsLogIndent(nsILog* log, const char* msg)
    : mLog(log), mHeaderMsg(msg)
{
    if (mHeaderMsg) mLog->Printf("[ Begin %s", mHeaderMsg);
    (void)mLog->IncreaseIndent();
}

nsLogIndent::~nsLogIndent()
{
    (void)mLog->DecreaseIndent();
    if (mHeaderMsg) mLog->Printf("] End   %s", mHeaderMsg);
}

////////////////////////////////////////////////////////////////////////////////
// nsFileLogEventSink 

nsFileLogEventSink::nsFileLogEventSink()
    : mName(nsnull),
      mOutput(nsnull),
      mBeginningOfLine(PR_TRUE),
      mCloseFile(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsFileLogEventSink::~nsFileLogEventSink()
{
    if (mCloseFile) {
        ::fclose(mOutput);
        mOutput = nsnull;
    }
    if (mName) nsCRT::free(mName);
}

NS_IMPL_ISUPPORTS2(nsFileLogEventSink, 
                   nsIFileLogEventSink,
                   nsILogEventSink)

NS_IMETHODIMP
nsFileLogEventSink::GetDestinationName(char* *result)
{
    *result = nsCRT::strdup(mName);
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsFileLogEventSink::Init(const char* filePath)
{
    FILE* filePtr;
    if (nsCRT::strcmp(filePath, "1") == 0) {
        filePtr = stdout;
    }
    else if (nsCRT::strcmp(filePath, "2") == 0) {
        filePtr = stderr;
    }
    else {
        filePtr = ::fopen(filePath, "w");
        if (filePtr == nsnull)
            return NS_ERROR_FAILURE;
        mCloseFile = PR_TRUE;
    }
    return InitFromFILE(filePath, filePtr);
}

NS_IMETHODIMP
nsFileLogEventSink::InitFromFILE(const char* name, FILE* filePtr)
{
    mOutput = filePtr;
    mName = nsCRT::strdup(name);
    return mName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsFileLogEventSink::Print(nsILog* log, const char* msg)
{
    nsresult rv;

    if (!NS_LOG_ENABLED(log))
        return NS_OK;

    nsAutoMonitor monitor(gLogMonitor);
    
    // do debug output first
#ifdef XP_WIN
    OutputDebugString(msg);
#elif defined(XP_MAC)
    {
#       define BUF_SIZE 1024
        char buf[BUF_SIZE];
        PRUint32 len =
            PR_snprintf(buf+1, BUF_SIZE-1, "%s", msg);
        buf[0] = (char) (len > 255 ? 255 : len);
        DebugStr(StringPtr(buf));
    }
#endif

    if (!mBeginningOfLine) {
        ::fputc('\n', mOutput);
        mBeginningOfLine = PR_TRUE;
    }

    // print preamble
    PRUint32 flags;
    rv = log->GetControlFlags(&flags);
    if (NS_FAILED(rv)) return rv;

    if (flags & nsILog::PRINT_THREAD_ID) {
        ::fprintf(mOutput, "%8p ", PR_CurrentThread());
        mBeginningOfLine = PR_FALSE;
    }

    if (flags & nsILog::PRINT_LOG_NAME) {
        char* name;
        rv = log->GetName(&name);
        ::fprintf(mOutput, "%-8.8s ",
                  flags & nsILog::PRINT_LOG_NAME ? name : "");
        nsCRT::free(name);
        mBeginningOfLine = PR_FALSE;
    }

    PRUint32 indentLevel;
    rv = log->GetIndentLevel(&indentLevel);
    if (NS_FAILED(rv)) return rv;
    do {
      indent:
        // do indentation
        for (PRUint32 i = 0; i < indentLevel; i++) {
            ::fprintf(mOutput, "|  ");
            mBeginningOfLine = PR_FALSE;
        }

        char c;
        while ((c = *msg++)) {
            switch (c) {
              case '\n':
                if (*msg == '\0') {
                    ::fputc('\n', mOutput);
                    mBeginningOfLine = PR_TRUE;
                    break;
                }
                else {
                    ::fprintf(mOutput, "\n                    ");
                    mBeginningOfLine = PR_FALSE;
                    goto indent;
                }
              default:
                ::fputc(c, mOutput);
                mBeginningOfLine = PR_FALSE;
            }
        }
    } while (0);

    if (!mBeginningOfLine) {
        ::fputc('\n', mOutput);
        mBeginningOfLine = PR_TRUE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFileLogEventSink::Flush(nsILog* log)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    ::fflush(mOutput);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

#else

PR_IMPLEMENT(nsILog*)
NS_GetLog(const char* name, PRUint32 controlFlags)
{
    return nsnull;
}

#endif // NS_ENABLE_LOGGING
