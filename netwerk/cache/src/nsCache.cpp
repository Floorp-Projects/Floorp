/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is nsCache.cpp, released March 18, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan  <gordon@netscape.com>
 *    Patrick C. Beard <beard@netscape.com>
 *    Darin Fisher     <darin@netscape.com>
 */

#include "nsCache.h"
#include "nsReadableUtils.h"
#include "nsDependentSubstring.h"
#include "nsString.h"


/**
 * Cache Service Utility Functions
 */

#if defined(PR_LOGGING)
PRLogModuleInfo * gCacheLog = nsnull;


void
CacheLogInit()
{
    if (gCacheLog) return;
    gCacheLog = PR_NewLogModule("cache");
    NS_ASSERTION(gCacheLog, "\nfailed to allocate cache log.\n");
}


void
CacheLogPrintPath(PRLogModuleLevel level, char * format, nsIFile * item)
{
    nsCAutoString path;
    nsresult rv = item->GetNativePath(path);
    if (NS_SUCCEEDED(rv)) {
        PR_LOG(gCacheLog, level, (format, path.get()));
    } else {
        PR_LOG(gCacheLog, level, ("GetNativePath failed: %x", rv));
    }
}

#endif


PRUint32
SecondsFromPRTime(PRTime prTime)
{
  PRInt64  microSecondsPerSecond, intermediateResult;
  PRUint32 seconds;

  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_DIV(intermediateResult, prTime, microSecondsPerSecond);
  LL_L2UI(seconds, intermediateResult);
  return seconds;
}


PRTime
PRTimeFromSeconds(PRUint32 seconds)
{
  PRInt64 microSecondsPerSecond, intermediateResult;
  PRTime  prTime;

  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_UI2L(intermediateResult, seconds);
  LL_MUL(prTime, intermediateResult, microSecondsPerSecond);
  return prTime;
}


nsresult
ClientIDFromCacheKey(const nsACString&  key, char ** result)
{
    nsresult  rv = NS_OK;
    *result = nsnull;

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
        NS_ASSERTION(PR_FALSE, "FindCharInRead failed to find ':'");
        rv = NS_ERROR_UNEXPECTED;
    }
    return rv;
}


nsresult
ClientKeyFromCacheKey(const nsACString& key, char ** result)
{
    nsresult  rv = NS_OK;
    *result = nsnull;

    nsReadingIterator<char> start;
    key.BeginReading(start);
        
    nsReadingIterator<char> end;
    key.EndReading(end);
        
    if (FindCharInReadable(':', start, end)) {
        ++start;  // advance past clientID ':' delimiter
        *result = ToNewCString( Substring(start, end));
        if (!*result) rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        NS_ASSERTION(PR_FALSE, "FindCharInRead failed to find ':'");
        rv = NS_ERROR_UNEXPECTED;
    }
    return rv;
}
