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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsNetworkLinkService.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsCRT.h"

#import <Cocoa/Cocoa.h>
#import <netinet/in.h>

NS_IMPL_ISUPPORTS2(nsNetworkLinkService,
                   nsINetworkLinkService,
                   nsIObserver)

nsNetworkLinkService::nsNetworkLinkService()
    : mLinkUp(PR_TRUE)
    , mStatusKnown(PR_FALSE)
    , mReachability(NULL)
    , mCFRunLoop(NULL)
{
}

nsNetworkLinkService::~nsNetworkLinkService()
{
}

NS_IMETHODIMP
nsNetworkLinkService::GetIsLinkUp(bool *aIsUp)
{
    *aIsUp = mLinkUp;
    return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::GetLinkStatusKnown(bool *aIsUp)
{
    *aIsUp = mStatusKnown;
    return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::GetLinkType(PRUint32 *aLinkType)
{
  NS_ENSURE_ARG_POINTER(aLinkType);

  // XXX This function has not yet been implemented for this platform
  *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::Observe(nsISupports *subject,
                              const char *topic,
                              const PRUnichar *data)
{
    if (!strcmp(topic, "xpcom-shutdown")) {
        Shutdown();
    }

    return NS_OK;
}

nsresult
nsNetworkLinkService::Init(void)
{
    nsresult rv;

    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = observerService->AddObserver(this, "xpcom-shutdown", PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    // If the network reachability API can reach 0.0.0.0 without
    // requiring a connection, there is a network interface available.
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    mReachability =
        ::SCNetworkReachabilityCreateWithAddress(NULL,
                                                 (struct sockaddr *)&addr);
    if (!mReachability) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    SCNetworkReachabilityContext context = {0, this, NULL, NULL, NULL};
    if (!::SCNetworkReachabilitySetCallback(mReachability,
                                            ReachabilityChanged,
                                            &context)) {
        NS_WARNING("SCNetworkReachabilitySetCallback failed.");
        ::CFRelease(mReachability);
        mReachability = NULL;
        return NS_ERROR_NOT_AVAILABLE;
    }

    // Get the current run loop.  This service is initialized at startup,
    // so we shouldn't run in to any problems with modal dialog run loops.
    mCFRunLoop = [[NSRunLoop currentRunLoop] getCFRunLoop];
    if (!mCFRunLoop) {
        NS_WARNING("Could not get current run loop.");
        ::CFRelease(mReachability);
        mReachability = NULL;
        return NS_ERROR_NOT_AVAILABLE;
    }
    ::CFRetain(mCFRunLoop);

    if (!::SCNetworkReachabilityScheduleWithRunLoop(mReachability, mCFRunLoop,
                                                    kCFRunLoopDefaultMode)) {
        NS_WARNING("SCNetworkReachabilityScheduleWIthRunLoop failed.");
        ::CFRelease(mReachability);
        mReachability = NULL;
        ::CFRelease(mCFRunLoop);
        mCFRunLoop = NULL;
        return NS_ERROR_NOT_AVAILABLE;
    }

    UpdateReachability();

    return NS_OK;
}

nsresult
nsNetworkLinkService::Shutdown()
{
    if (!::SCNetworkReachabilityUnscheduleFromRunLoop(mReachability,
                                                      mCFRunLoop,
                                                      kCFRunLoopDefaultMode)) {
        NS_WARNING("SCNetworkReachabilityUnscheduleFromRunLoop failed.");
    }

    ::CFRelease(mReachability);
    mReachability = NULL;

    ::CFRelease(mCFRunLoop);
    mCFRunLoop = NULL;

    return NS_OK;
}

void
nsNetworkLinkService::UpdateReachability()
{
    if (!mReachability) {
        return;
    }

    SCNetworkConnectionFlags flags;
    if (!::SCNetworkReachabilityGetFlags(mReachability, &flags)) {
        mStatusKnown = PR_FALSE;
        return;
    }

    bool reachable = (flags & kSCNetworkFlagsReachable) != 0;
    bool needsConnection = (flags & kSCNetworkFlagsConnectionRequired) != 0;

    mLinkUp = (reachable && !needsConnection);
    mStatusKnown = PR_TRUE;
}

void
nsNetworkLinkService::SendEvent()
{
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1");
    if (!observerService)
        return;

    const char *event;
    if (!mStatusKnown)
        event = NS_NETWORK_LINK_DATA_UNKNOWN;
    else
        event = mLinkUp ? NS_NETWORK_LINK_DATA_UP
                        : NS_NETWORK_LINK_DATA_DOWN;

    observerService->NotifyObservers(static_cast<nsINetworkLinkService*>(this),
                                     NS_NETWORK_LINK_TOPIC,
                                     NS_ConvertASCIItoUTF16(event).get());
}

/* static */
void
nsNetworkLinkService::ReachabilityChanged(SCNetworkReachabilityRef target,
                                          SCNetworkConnectionFlags flags,
                                          void *info)
{
    nsNetworkLinkService *service =
        static_cast<nsNetworkLinkService*>(info);

    service->UpdateReachability();
    service->SendEvent();
}
