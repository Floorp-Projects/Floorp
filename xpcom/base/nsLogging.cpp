/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 */

#include "nsLogging.h"

#ifdef NS_ENABLE_LOGGING

#include "nsCRT.h"
#include "prthread.h"
#include "nsAutoLock.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsSpecialSystemDirectory.h"
#include <math.h>
#ifdef XP_PC
#include <windows.h>
#endif

#undef printf
#undef fprintf

static PRMonitor* gLogMonitor = nsnull;
static nsObjectHashtable* gSettings = nsnull;

NS_DECL_LOG(LogInfo);

NS_DEFINE_CID(kLoggingServiceCID, NS_LOGGINGSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////
// nsLoggingService

nsLoggingService::nsLoggingService()
    : mDefaultControlFlags(nsILog::PRINT_THREAD_ID |
                           nsILog::PRINT_LOG_NAME |
                           nsILog::PRINT_LEVEL |
                           nsILog::TIMING_PER_THREAD),
      mLogs(16)
{
    NS_INIT_REFCNT();
}

nsLoggingService::~nsLoggingService()
{
    NS_INIT_LOG(LogInfo);
    DescribeLogs(LogInfo);
    DescribeTimings(LogInfo);
}

NS_IMPL_ISUPPORTS1(nsLoggingService, nsILoggingService)

static void*
levelClone(nsHashKey *aKey, void *aData, void* closure)
{
    PRUint32 level = (PRUint32)aData;
    return (void*)level;
}

static PRBool
levelDestroy(nsHashKey *aKey, void *aData, void* closure)
{
    return PR_TRUE;
}

static void
RecordSetting(const char* name, const char* value)
{
    PRUint32 level = nsILog::LEVEL_ERROR;
    if (nsCRT::strcasecmp(value, "ERROR") == 0 ||
        nsCRT::strcasecmp(value, "2") == 0) {
        level = nsILog::LEVEL_ERROR;
        fprintf(stderr, "### NS_LOG: %s = ERROR\n", name);
    }
    else if (nsCRT::strcasecmp(value, "WARN") == 0 ||
             nsCRT::strcasecmp(value, "WARNING") == 0 ||
             nsCRT::strcasecmp(value, "3") == 0) {
        level = nsILog::LEVEL_WARN;
        fprintf(stderr, "### NS_LOG: %s = WARN\n", name);
    }
    else if (nsCRT::strcasecmp(value, "STDOUT") == 0 ||
             nsCRT::strcasecmp(value, "OUT") == 0 ||
             nsCRT::strcasecmp(value, "4") == 0) {
        level = nsILog::LEVEL_STDOUT;
        fprintf(stderr, "### NS_LOG: %s = STDOUT\n", name);
    }
    else if (nsCRT::strcasecmp(value, "DBG") == 0 ||
             nsCRT::strcasecmp(value, "DEBUG") == 0 ||
             nsCRT::strcasecmp(value, "5") == 0) {
        level = nsILog::LEVEL_DBG;
        fprintf(stderr, "### NS_LOG: %s = DBG\n", name);
    }
    else {
        fprintf(stderr, "### NS_LOG error: %s = %s (bad level)\n", name, value);
    }

    nsStringKey key(name);
    gSettings->Put(&key, (void*)level);
}

nsresult
nsLoggingService::Init()
{
    nsresult rv;
    nsStandardLogEventSink* defaultSink = nsnull;
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
            fprintf(stderr, "### NS_LOG: using NSPR_LOG_MODULES (instead of .nslog)\n");
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
            fprintf(stderr, "### NS_LOG: using NSPR_LOG_FILE (instead of .nslog) -- logging to %s\n", 
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

    defaultSink = new nsStandardLogEventSink();
    if (defaultSink == nsnull) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto error;
    }

    NS_ADDREF(defaultSink);
    if (outputPath)
        rv = defaultSink->Init(outputPath, nsILog::LEVEL_ERROR);
    else
        rv = defaultSink->InitFromFILE("stderr", stderr, nsILog::LEVEL_ERROR);
    if (NS_FAILED(rv)) goto error;

    mDefaultSink = defaultSink;
    NS_RELEASE(defaultSink);
    return NS_OK;

  error:
    NS_IF_RELEASE(defaultSink);
    if (gLogMonitor) {
        PR_DestroyMonitor(gLogMonitor);
        gLogMonitor = nsnull;
    }
    return rv;
}

NS_METHOD
nsLoggingService::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    nsresult rv;
    if (outer)
        return NS_ERROR_NO_AGGREGATION;

    nsLoggingService* it = new nsLoggingService();
    if (it == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = it->Init();
    if (NS_FAILED(rv)) {
        delete it;
        return rv; 
    }

    rv = it->QueryInterface(aIID, aInstancePtr);
    if (NS_FAILED(rv)) {
        delete it;
        return rv;
    }
    return rv;
}

NS_IMETHODIMP
nsLoggingService::GetLog(const char* name, nsILog* *result)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    nsStringKey key(name);
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
    mLogs.Put(&key, newLog);
    *result = newLog;
    NS_ADDREF(newLog);
    return NS_OK;
}

static PRBool
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
    NS_LOG(out, STDOUT, ("%-20.20s %-8.8s %s\n", "LOG NAME", "ENABLED", "DESTINATION"));
    mLogs.Enumerate(DescribeLog, out);
    return NS_OK;
}

static PRBool
DescribeTiming(nsHashKey *aKey, void *aData, void* closure)
{
    nsILog* log = (nsILog*)aData;
    nsILog* out = (nsILog*)closure;
    nsresult rv;
    PRUint32 sampleSize;
    double meanTime;
    double stdDevTime;
    rv = log->GetTimingStats(&sampleSize, &meanTime, &stdDevTime);
    if (NS_SUCCEEDED(rv) && sampleSize > 0)
        (void)log->DescribeTiming(out, "TOTAL TIME");
    return PR_TRUE;
}

NS_IMETHODIMP
nsLoggingService::DescribeTimings(nsILog* out)
{
    NS_LOG(out, STDOUT, ("  %-20.20s %-8.8s %s\n", "LOG NAME", "ENABLED", "DESTINATION"));
    mLogs.Enumerate(DescribeTiming, out);
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
      mControlFlags(0),
      mIndentLevel(0)
{
    mEnabledLevel = LEVEL_ERROR;
    NS_INIT_REFCNT();
}

nsLog::~nsLog()
{
    if (mName) nsCRT::free(mName);
}

NS_IMPL_ISUPPORTS1(nsLog, nsILog)

static void PR_CALLBACK
DeleteTimingData(void *priv)
{
    nsTimingData* data = (nsTimingData*)priv;
    delete data;
}

nsresult
nsLog::Init(const char* name, PRUint32 controlFlags, nsILogEventSink* sink)
{
    mName = nsCRT::strdup(name);
    if (mName == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    mControlFlags = controlFlags; 
    mSink = sink;

    nsStringKey key(name);
    PRUint32 level = (PRUint32)gSettings->Get(&key);
    if (level != LEVEL_NEVER) {
        mEnabledLevel = level;
    }
    PRStatus status = PR_NewThreadPrivateIndex(&mThreadTimingDataIndex,
                                               DeleteTimingData);
    if (status != PR_SUCCESS)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

NS_IMETHODIMP
nsLog::GetName(char* *aName)
{
    *aName = nsCRT::strdup(mName);
    return *aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsLog::GetLevel(PRUint32 *aLevel)
{
    *aLevel = mEnabledLevel;
    return NS_OK;
}

NS_IMETHODIMP
nsLog::SetLevel(PRUint32 aLevel)
{
    mEnabledLevel = aLevel;
    return NS_OK;
}

NS_IMETHODIMP
nsLog::Enabled(PRUint32 level, PRBool *result)
{
    *result = Test(level);
    return NS_OK;
}

NS_IMETHODIMP
nsLog::Print(PRUint32 level, const PRUnichar *message)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    nsCString str(message);
    char* msg = str.ToNewCString();
    nsLogEvent event(this, level);
    nsresult rv = event.Printf(msg);
    nsCRT::free(msg);
    return rv;
}

NS_IMETHODIMP
nsLog::Flush(void)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    return mSink->Flush();
}

NS_IMETHODIMP
nsLog::PrintEvent(nsLogEvent& event)
{
    nsAutoMonitor monitor(gLogMonitor);
    
    return mSink->PrintEvent(event);
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
    
//    PR_ASSERT(mIndentLevel > 0);      // XXX layout is having trouble
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
    const char* levelName;
    switch (mEnabledLevel) {
      case nsILog::LEVEL_NEVER:  levelName = "NEVER"; break;
      case nsILog::LEVEL_ERROR:  levelName = "ERROR"; break;
      case nsILog::LEVEL_WARN:   levelName = "WARN"; break;
      case nsILog::LEVEL_STDOUT: levelName = "STDOUT"; break;
      case nsILog::LEVEL_DBG:    levelName = "DBG"; break;
      default:                   levelName = "<unknown>"; break;
    }
    char* dest = nsnull;
    rv = mSink->GetDestinationName(&dest);
    if (NS_FAILED(rv)) {
        dest = nsCRT::strdup("<unknown>");
    }
    NS_LOG(out, STDOUT, ("  %-20.20s %-8.8s %s\n", mName, levelName, dest));
    if (dest) nsCRT::free(dest);
    return NS_OK;
}

NS_IMETHODIMP
nsLog::BeginTiming(void)
{
    nsTimingData* data;
    if (mControlFlags & TIMING_PER_THREAD) {
        data = (nsTimingData*)PR_GetThreadPrivate(mThreadTimingDataIndex);
        if (data == nsnull) {
            data = new nsTimingData;
            if (data == nsnull) 
                return NS_ERROR_OUT_OF_MEMORY;
            PRStatus status = PR_SetThreadPrivate(mThreadTimingDataIndex, data);
            if (status != PR_SUCCESS)
                return NS_ERROR_FAILURE;
        }
    }
    else {
        data = &mTimingData;
    } 

    PR_ASSERT(data->mStartTime == 0);
    if (data->mStartTime != 0)
        return NS_ERROR_FAILURE;
    data->mStartTime = PR_IntervalNow();
    return NS_OK;
}

NS_IMETHODIMP
nsLog::EndTiming(PRIntervalTime *elapsedTime)
{
    nsTimingData* data;
    if (mControlFlags & TIMING_PER_THREAD)
        data = (nsTimingData*)PR_GetThreadPrivate(mThreadTimingDataIndex);
    else
        data = &mTimingData;
    
    PR_ASSERT(data->mStartTime != 0);
    if (data->mStartTime == 0)
        return NS_ERROR_FAILURE;
    PRIntervalTime elapsed = PR_IntervalNow();
    elapsed -= data->mStartTime;
    data->mStartTime = 0;
    data->mTimingSamples++;
    data->mTotalTime += elapsed;
    data->mTotalSquaredTime += elapsed * elapsed;
    *elapsedTime = elapsed;

    if (mControlFlags & TIMING_PER_THREAD) {
        // dump per-thread data into per-log data:
        mTimingData.mTotalTime += data->mTotalTime;
        mTimingData.mTotalSquaredTime += data->mTotalSquaredTime;
        mTimingData.mTimingSamples += data->mTimingSamples;

        // destroy TLS:
        PRStatus status = PR_SetThreadPrivate(mThreadTimingDataIndex, nsnull);
        if (status != PR_SUCCESS)
            return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsLog::GetTimingStats(PRUint32 *sampleSize,
                      double *meanTime,
                      double *stdDevTime)
{
    *sampleSize = mTimingData.mTimingSamples;
    double mean = mTimingData.mTotalTime / mTimingData.mTimingSamples;
    *meanTime = (PRIntervalTime)mean;
    double variance = fabs(mTimingData.mTotalSquaredTime / mTimingData.mTimingSamples - mean * mean);
    *stdDevTime = sqrt(variance);
    return NS_OK;
}

NS_IMETHODIMP
nsLog::DescribeTiming(nsILog* out, const char* msg)
{
    nsresult rv;
    
    PR_ASSERT(mTimingData.mStartTime == 0);
    if (mTimingData.mStartTime != 0) {
        NS_LOG(this, STDOUT, ("%s: TIMING ERROR\n", msg));
        return NS_ERROR_FAILURE;
    }

    PRUint32 realTimeSamples;
    double realTimeMean, realTimeStdDev;
    rv = GetTimingStats(&realTimeSamples, &realTimeMean, &realTimeStdDev);
    if (NS_FAILED(rv)) return rv;
    PRUint32 tps = PR_TicksPerSecond();
    if (realTimeSamples > 1) {
        NS_LOG(out, STDOUT, ("%s: %.2f +/- %.2f ms (%d samples)\n", 
                             msg,
                             realTimeMean * 1000 / tps,
                             realTimeStdDev * 1000 / tps,
                             realTimeSamples));
    }
    else {
        NS_LOG(out, STDOUT, ("%s: %.2f ms\n", 
                             msg, realTimeMean * 1000 / tps));
    }
    return NS_OK;
}

NS_IMETHODIMP
nsLog::ResetTiming(void)
{
    PR_ASSERT(mTimingData.mStartTime == 0);
    mTimingData.mTotalTime = 0;
    mTimingData.mTotalSquaredTime = 0;
    mTimingData.mTimingSamples = 0;
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
// nsLogEvent

nsresult
nsLogEvent::Printf(const char* format, ...)
{
    if (mMessage == nsnull) {
        va_list args;
        va_start(args, format);
        mMessage = PR_vsmprintf(format, args);
        va_end(args);
    }
    return mLog->PrintEvent(*this);
}

nsresult
nsLogEvent::Vprintf(const char* format, va_list args)
{
    if (mMessage == nsnull) {
        mMessage = PR_vsmprintf(format, args);
    }
    return mLog->PrintEvent(*this);
}

////////////////////////////////////////////////////////////////////////////////
// nsStandardLogEventSink 

nsStandardLogEventSink::nsStandardLogEventSink()
    : mName(nsnull),
      mOutput(nsnull),
      mDebugLevel(nsILog::LEVEL_NEVER),
      mBeginningOfLine(PR_TRUE),
      mCloseFile(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsStandardLogEventSink::~nsStandardLogEventSink()
{
    if (mCloseFile) {
        ::fclose(mOutput);
        mOutput = nsnull;
    }
    if (mName) nsCRT::free(mName);
}

NS_IMPL_ISUPPORTS2(nsStandardLogEventSink, 
                   nsIStandardLogEventSink,
                   nsILogEventSink)

NS_IMETHODIMP
nsStandardLogEventSink::GetDestinationName(char* *result)
{
    *result = nsCRT::strdup(mName);
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsStandardLogEventSink::Init(const char* filePath, PRUint32 levelForDebugOutput)
{
    FILE* filePtr;
    if (nsCRT::strcmp(filePath, "1") == 0) {
        filePtr = stdout;
    }
    else if (nsCRT::strcmp(filePath, "2") == 0) {
        filePtr = stderr;
    }
    else {
        filePtr = ::fopen(filePath, "W");
        if (filePtr == nsnull)
            return NS_ERROR_FAILURE;
        mCloseFile = PR_TRUE;
    }
    return InitFromFILE(filePath, filePtr, levelForDebugOutput);
}

NS_IMETHODIMP
nsStandardLogEventSink::InitFromFILE(const char* name, FILE* filePtr, PRUint32 levelForDebugOutput)
{
    mOutput = filePtr;
    mDebugLevel = levelForDebugOutput;
    mName = nsCRT::strdup(name);
    return mName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsStandardLogEventSink::PrintEvent(nsLogEvent& event)
{
    nsresult rv;
    nsILog* log = event.GetLog();
    PRUint32 level = event.GetLevel();
    const char* msg = event.GetMsg();

    if (level == nsILog::LEVEL_NEVER) 
        return NS_OK;

    // do debug output first
    if (level <= mDebugLevel) {
#ifdef XP_PC
        OutputDebugString(msg);
#elif defined(XP_MAC)
        {
#           define BUF_SIZE 1024
            char buf[BUF_SIZE];
            PRUint32 len =
                PL_snprintf(buf+1, BUF_SIZE-1, "ERROR: %s", msg);
            buf[0] = (char) (len > 255 ? 255 : len);
            DebugStr(StringPtr(buf));
        }
#endif
    }

    if (!log->Test(level))
        return NS_OK;

    nsAutoMonitor monitor(gLogMonitor);
    
    if (!mBeginningOfLine) {
        ::fputc('\n', mOutput);
    }

    // print preamble
    char levels[] = { 'X', 'E', 'W', ' ', 'D' };
    PRUint32 flags;
    rv = log->GetControlFlags(&flags);
    if (NS_FAILED(rv)) return rv;
    if (flags & nsILog::PRINT_THREAD_ID) {
        ::fprintf(mOutput, "%8x ", PR_CurrentThread());
    }
    else {
        ::fprintf(mOutput, "%8s ", "");
    }

    char* name;
    rv = log->GetName(&name);
    ::fprintf(mOutput, "%-8.8s %c ",
              flags & nsILog::PRINT_LOG_NAME ? name : "",
              flags & nsILog::PRINT_LEVEL ? levels[level] : ' ');
    nsCRT::free(name);
    mBeginningOfLine = PR_FALSE;

    PRUint32 indentLevel;
    rv = log->GetIndentLevel(&indentLevel);
    if (NS_FAILED(rv)) return rv;
    do {
      indent:
        // do indentation
        for (PRUint32 i = 0; i < indentLevel; i++) {
            ::fputs("|  ", mOutput);
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
                    ::fputs("\n                    ", mOutput);
                    goto indent;
                }
              default:
                ::fputc(c, mOutput);
            }
        }
    } while (0);
    return NS_OK;
}

NS_IMETHODIMP
nsStandardLogEventSink::Flush()
{
    ::fflush(mOutput);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

#endif // NS_ENABLE_LOGGING
