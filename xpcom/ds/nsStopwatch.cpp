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

#include "nsStopwatch.h"

#ifdef  NS_ENABLE_STOPWATCH

#include "nsILoggingService.h"
#include "prthread.h"
#include <math.h>

extern NS_DECL_LOG(LogInfo);

////////////////////////////////////////////////////////////////////////////////
// nsStopwatchService

nsStopwatchService::nsStopwatchService()
    : mStopwatches(16)
{
    NS_INIT_REFCNT();
}

nsStopwatchService::~nsStopwatchService()
{
    DescribeTimings(LogInfo);
}

NS_IMPL_ISUPPORTS1(nsStopwatchService, nsIStopwatchService)

nsresult
nsStopwatchService::Init()
{
    return NS_OK;
}

NS_METHOD
nsStopwatchService::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    nsresult rv;
    if (outer)
        return NS_ERROR_NO_AGGREGATION;

    nsStopwatchService* sw = new nsStopwatchService();
    if (sw == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = sw->Init();
    if (NS_FAILED(rv)) {
        delete sw;
        return rv; 
    }

    rv = sw->QueryInterface(aIID, aInstancePtr);
    if (NS_FAILED(rv)) {
        delete sw;
        return rv;
    }
    return rv;
}

NS_IMETHODIMP
nsStopwatchService::CreateStopwatch(const char* name, const char* countUnits, 
                                    PRBool perThread, nsIStopwatch* *result)
{
    nsStopwatch* sw = new nsStopwatch();
    if (sw == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(sw);
    nsresult rv = sw->Init(name, countUnits, perThread);
    if (NS_FAILED(rv)) goto done;
    rv = Define(name, sw);
    if (NS_FAILED(rv)) goto done;
    *result = sw;
    NS_ADDREF(*result);
  done:
    NS_RELEASE(sw);
    return rv;
}

NS_IMETHODIMP
nsStopwatchService::Define(const char *prop, nsISupports *initialValue)
{
    nsStringKey key(prop);
    if (mStopwatches.Put(&key, initialValue))
    {
        NS_ASSERTION(0, "stopwatch redefinition");
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStopwatchService::Undefine(const char *prop)
{
    nsStringKey key(prop);
    if (!mStopwatches.Remove(&key))
    {
        NS_ASSERTION(0, "stopwatch undefined");
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStopwatchService::Get(const char *prop, const nsIID & uuid, void * *result)
{
    nsStringKey key(prop);
    nsCOMPtr<nsISupports> sw = (nsStopwatch*)mStopwatches.Get(&key);
    NS_ASSERTION(sw != nsnull, "stopwatch undefined");
    if (sw == nsnull)
        return NS_ERROR_FAILURE;
    return sw->QueryInterface(uuid, result);
}

NS_IMETHODIMP
nsStopwatchService::Set(const char *prop, nsISupports *value)
{
    nsStringKey key(prop);
    if (!mStopwatches.Put(&key, value))
    {
        NS_ASSERTION (0, "stopwatch undefined");
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStopwatchService::Has(const char *prop, const nsIID & uuid, nsISupports *value, 
                        PRBool *result)
{
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsStopwatch

nsStopwatch::nsStopwatch()
    : mName(nsnull),
      mCountUnits(nsnull),
      mPerThread(PR_TRUE),
      mThreadTimingDataIndex(0)
{
    NS_INIT_REFCNT();
}

nsStopwatch::~nsStopwatch()
{
    if (mName) nsCRT::free(mName);
    if (mCountUnits) nsCRT::free(mCountUnits);
}

NS_IMPL_ISUPPORTS1(nsStopwatch, nsIStopwatch)

NS_METHOD
nsStopwatch::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    nsresult rv;
    if (outer)
        return NS_ERROR_NO_AGGREGATION;

    nsStopwatch* sw = new nsStopwatch();
    if (sw == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(sw);
    rv = sw->QueryInterface(aIID, aInstancePtr);
    NS_RELEASE(sw);
    return rv;
}

void 
nsStopwatch::TimeUnits(double timeInMilliSeconds,
                       double *adjustedValue, const char* *adjustedUnits,
                       double *factorResult)
{
    double time = timeInMilliSeconds;
    const char* units = "ms";
    double factor = 1;
    if (time < 1) {
        time *= 1000;
        factor *= 1000;
        units = "us";
        if (time < 1) {
            time *= 1000;
            factor *= 1000;
            units = "ns";
        }
    }
    else if (time > 1000) {
        time /= 1000;
        factor /= 1000;
        units = "sec";
        if (time > 60) {
            time /= 60;
            factor /= 60;
            units = "min";
            if (time > 60) {
                time /= 60;
                factor /= 60;
                units = "hours";
                if (time > 24) {
                    time /= 24;
                    factor /= 24;
                    units = "days";
                }
            }
        }
    }
    *adjustedValue = time;
    *adjustedUnits = units;
    *factorResult = factor;
}

////////////////////////////////////////////////////////////////////////////////

static void PR_CALLBACK
DeleteTimingData(void *priv)
{
    nsTimingData* data = (nsTimingData*)priv;
    delete data;
}

NS_IMETHODIMP
nsStopwatch::Init(const char *name, const char* countUnits, PRBool perThread)
{
    if (mName) nsCRT::free(mName);
    if (mCountUnits) nsCRT::free(mCountUnits);
    Reset();

    mName = nsCRT::strdup(name);
    if (mName == nsnull) 
        return NS_ERROR_OUT_OF_MEMORY;
    mCountUnits = nsCRT::strdup(countUnits);
    if (mCountUnits == nsnull) 
        return NS_ERROR_OUT_OF_MEMORY;
    mPerThread = perThread;
    PRStatus status = PR_NewThreadPrivateIndex(&mThreadTimingDataIndex,
                                               DeleteTimingData);
    if (status != PR_SUCCESS)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

NS_IMETHODIMP
nsStopwatch::GetName(char * *aName)
{
    *aName = nsCRT::strdup(mName);
    return *aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsStopwatch::Start(void)
{
    nsTimingData* data;
    if (mPerThread) {
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
nsStopwatch::Stop(double count, PRIntervalTime *elapsedTime)
{
    nsTimingData* data;
    if (mPerThread)
        data = (nsTimingData*)PR_GetThreadPrivate(mThreadTimingDataIndex);
    else
        data = &mTimingData;
    
    PR_ASSERT(data->mStartTime != 0);
    if (data->mStartTime == 0)
        return NS_ERROR_FAILURE;
    PRIntervalTime elapsed = PR_IntervalNow();
    elapsed -= data->mStartTime;
    data->mStartTime = 0;
    data->mCount++;
    data->mTotalTime += elapsed;
    data->mTotalSquaredTime += elapsed * elapsed;
    *elapsedTime = elapsed;

    if (mPerThread) {
        // dump per-thread data into per-log data:
        mTimingData.mTotalTime += data->mTotalTime;
        mTimingData.mTotalSquaredTime += data->mTotalSquaredTime;
        mTimingData.mCount += data->mCount;

        // destroy TLS:
        PRStatus status = PR_SetThreadPrivate(mThreadTimingDataIndex, nsnull);
        if (status != PR_SUCCESS)
            return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStopwatch::Reset(void)
{
    mTimingData.mStartTime = 0;
    mTimingData.mTotalTime = 0;
    mTimingData.mTotalSquaredTime = 0;
    mTimingData.mCount = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsStopwatch::GetRealTimeStats(double *realTimeSamples, double *realTimeMean, double *realTimeStdDev)
{
    PRUint32 tps = PR_TicksPerSecond();

    *realTimeSamples = mTimingData.mCount;
    double mean = mTimingData.mTotalTime / mTimingData.mCount;
    *realTimeMean = (PRIntervalTime)mean * 1000 / tps;
    double variance = fabs(mTimingData.mTotalSquaredTime / mTimingData.mCount - mean * mean);
    *realTimeStdDev = sqrt(variance) * 1000 / tps;
    return NS_OK;
}

NS_IMETHODIMP
nsStopwatch::GetCPUTimeStats(double *cpuTimeSamples, double *cpuTimeMean, double *cpuTimeStdDev)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStopwatch::Describe(nsILog *out, const char* msg)
{
    nsresult rv;

    double realTimeSamples, realTimeMean, realTimeStdDev;
    rv = GetRealTimeStats(&realTimeSamples, &realTimeMean, &realTimeStdDev);
    if (NS_FAILED(rv)) return rv;

    double mean, factor, stdDev;
    const char* timeUnits;
    TimeUnits(realTimeMean, &mean, &timeUnits, &factor);
    stdDev = realTimeStdDev * factor;

    if (realTimeSamples > 1) {
        NS_LOG(out, STDOUT, ("%s elapsed time: %.2f +/- %.2f %s (%d %s) [%s]\n", 
                             mName, mean, stdDev, timeUnits,
                             (PRUint32)realTimeSamples, mCountUnits, msg));
    }
    else {
        NS_LOG(out, STDOUT, ("%s elapsed time: %.2f %s [%s]\n", 
                             mName, mean, timeUnits, msg));
    }

    TimeUnits(1/realTimeMean, &mean, &timeUnits, &factor);
    NS_LOG(out, STDOUT, ("==> %.2f %s/%s\n", mean, mCountUnits, timeUnits));

    double cpuTimeSamples, cpuTimeMean, cpuTimeStdDev;
    rv = GetCPUTimeStats(&cpuTimeSamples, &cpuTimeMean, &cpuTimeStdDev);
    if (NS_SUCCEEDED(rv)) {
        TimeUnits(cpuTimeMean, &mean, &timeUnits, &factor);
        stdDev = cpuTimeStdDev * factor;
        NS_LOG(out, STDOUT, ("%s cpu time: %.2f +/- %.2f %s (%d %s)\n", 
                             mName, mean, stdDev, timeUnits,
                             cpuTimeSamples, mCountUnits));
    }

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

#endif // NS_ENABLE_STOPWATCH
