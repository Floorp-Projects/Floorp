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

#include "nsTimingService.h"

NS_IMPL_ISUPPORTS1(nsTimingService, nsITimingService)

nsTimingService::~nsTimingService()
{
    // free up entries in hashtable
}
    
NS_IMETHODIMP
nsTimingService::StartTimer(const char *aTimerName)
{
    PRTime now = PR_Now();

    return SetTimer(aTimerName, now);
}

NS_IMETHODIMP
nsTimingService::SetTimer(const char *aTimerName, PRInt64 aTime)
{
    nsCStringKey timerKey(aTimerName);

    PRInt64 *aTimerEntry = new PRInt64(aTime);
    if (!aTimerEntry) return NS_ERROR_OUT_OF_MEMORY;

#if 0
    PRInt64 *oldEntry =
        NS_STATIC_CAST(PRInt64 *, mTimers.Put(&timerKey,
                                              NS_STATIC_CAST(void *, aTimerEntry)));
#endif
    mTimers.Put(&timerKey, (void *)aTimerEntry);
#if 0
    if (oldEntry)
        delete oldEntry;
#endif
    
    return NS_OK;
}

NS_IMETHODIMP
nsTimingService::GetElapsedTime(const char *aTimerName, PRInt64* aResult)
{
    PRTime now = PR_Now();

    nsCStringKey timerKey(aTimerName);
    
    PRInt64 *then = (PRInt64 *)mTimers.Get(&timerKey);
    if (!then) return NS_ERROR_FAILURE;
    
    LL_SUB(*aResult, now, *then);

    return NS_OK;
}
