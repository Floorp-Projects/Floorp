/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick McManus <mcmanus@ducksong.com>
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

#include "nsDNSPrefetch.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsNetUtil.h"

#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"
#include "nsIDNSService.h"
#include "nsICancelable.h"

static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
static nsIDNSService *sDNSService = nsnull;

nsresult
nsDNSPrefetch::Initialize(nsIDNSService *aDNSService)
{
    NS_IF_RELEASE(sDNSService);
    sDNSService =  aDNSService;
    NS_IF_ADDREF(sDNSService);
    return NS_OK;
}

nsresult
nsDNSPrefetch::Shutdown()
{
    NS_IF_RELEASE(sDNSService);
    return NS_OK;
}

nsDNSPrefetch::nsDNSPrefetch(nsIURI *aURI, bool storeTiming)
    : mStoreTiming(storeTiming)
{
    aURI->GetAsciiHost(mHostname);
}

nsresult 
nsDNSPrefetch::Prefetch(PRUint16 flags)
{
    if (mHostname.IsEmpty())
        return NS_ERROR_NOT_AVAILABLE;
  
    if (!sDNSService)
        return NS_ERROR_NOT_AVAILABLE;
    
    nsCOMPtr<nsICancelable> tmpOutstanding;  

    if (mStoreTiming)
        mStartTimestamp = mozilla::TimeStamp::Now();
    // If AsyncResolve fails, for example because prefetching is disabled,
    // then our timing will be useless. However, in such a case,
    // mEndTimestamp will be a null timestamp and callers should check
    // TimingsValid() before using the timing.
    return sDNSService->AsyncResolve(mHostname, flags | nsIDNSService::RESOLVE_SPECULATE,
                                     this, nsnull, getter_AddRefs(tmpOutstanding));
}

nsresult
nsDNSPrefetch::PrefetchLow()
{
    return Prefetch(nsIDNSService::RESOLVE_PRIORITY_LOW);
}

nsresult
nsDNSPrefetch::PrefetchMedium()
{
    return Prefetch(nsIDNSService::RESOLVE_PRIORITY_MEDIUM);
}

nsresult
nsDNSPrefetch::PrefetchHigh()
{
    return Prefetch(0);
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsDNSPrefetch, nsIDNSListener)

NS_IMETHODIMP
nsDNSPrefetch::OnLookupComplete(nsICancelable *request,
                                nsIDNSRecord  *rec,
                                nsresult       status)
{
    if (mStoreTiming)
        mEndTimestamp = mozilla::TimeStamp::Now();
    return NS_OK;
}
