/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCache.h"
#include "nsReadableUtils.h"
#include "nsDependentSubstring.h"
#include "nsString.h"


/**
 * Cache Service Utility Functions
 */

mozilla::LazyLogModule gCacheLog("cache");

void
CacheLogPrintPath(mozilla::LogLevel level, const char * format, nsIFile * item)
{
    nsAutoCString path;
    nsresult rv = item->GetNativePath(path);
    if (NS_SUCCEEDED(rv)) {
        MOZ_LOG(gCacheLog, level, (format, path.get()));
    } else {
        MOZ_LOG(gCacheLog, level, ("GetNativePath failed: %x", rv));
    }
}


uint32_t
SecondsFromPRTime(PRTime prTime)
{
  int64_t  microSecondsPerSecond = PR_USEC_PER_SEC;
  return uint32_t(prTime / microSecondsPerSecond);
}


PRTime
PRTimeFromSeconds(uint32_t seconds)
{
  int64_t intermediateResult = seconds;
  PRTime prTime = intermediateResult * PR_USEC_PER_SEC;
  return prTime;
}


nsresult
ClientIDFromCacheKey(const nsACString&  key, char ** result)
{
    nsresult  rv = NS_OK;
    *result = nullptr;

    nsReadingIterator<char> colon;
    key.BeginReading(colon);
        
    nsReadingIterator<char> start;
    key.BeginReading(start);
        
    nsReadingIterator<char> end;
    key.EndReading(end);
        
    if (FindCharInReadable(':', colon, end)) {
        *result = ToNewCString( Substring(start, colon));
        if (!*result) rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        NS_ASSERTION(false, "FindCharInRead failed to find ':'");
        rv = NS_ERROR_UNEXPECTED;
    }
    return rv;
}


nsresult
ClientKeyFromCacheKey(const nsCString& key, nsACString &result)
{
    nsresult  rv = NS_OK;

    nsReadingIterator<char> start;
    key.BeginReading(start);
        
    nsReadingIterator<char> end;
    key.EndReading(end);
        
    if (FindCharInReadable(':', start, end)) {
        ++start;  // advance past clientID ':' delimiter
        result.Assign(Substring(start, end));
    } else {
        NS_ASSERTION(false, "FindCharInRead failed to find ':'");
        rv = NS_ERROR_UNEXPECTED;
        result.Truncate(0);
    }
    return rv;
}
