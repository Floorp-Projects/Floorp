/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNetworkLinkService.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsCRT.h"

#import <Cocoa/Cocoa.h>
#import <netinet/in.h>

NS_IMPL_ISUPPORTS(nsNetworkLinkService,
                  nsINetworkLinkService,
                  nsIObserver)

nsNetworkLinkService::nsNetworkLinkService()
    : mLinkUp(true)
    , mStatusKnown(false)
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
nsNetworkLinkService::GetLinkType(uint32_t *aLinkType)
{
  NS_ENSURE_ARG_POINTER(aLinkType);

  // XXX This function has not yet been implemented for this platform
  *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::Observe(nsISupports *subject,
                              const char *topic,
                              const char16_t *data)
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

    rv = observerService->AddObserver(this, "xpcom-shutdown", false);
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
        mStatusKnown = false;
        return;
    }

    bool reachable = (flags & kSCNetworkFlagsReachable) != 0;
    bool needsConnection = (flags & kSCNetworkFlagsConnectionRequired) != 0;

    mLinkUp = (reachable && !needsConnection);
    mStatusKnown = true;
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
