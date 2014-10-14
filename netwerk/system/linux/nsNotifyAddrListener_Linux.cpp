/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et sw=4 ts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdarg.h>
#include <fcntl.h>
#include <poll.h>

#include "nsThreadUtils.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsNotifyAddrListener_Linux.h"
#include "nsString.h"
#include "nsAutoPtr.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

#define NETWORK_NOTIFY_CHANGED_PREF "network.notify.changed"


NS_IMPL_ISUPPORTS(nsNotifyAddrListener,
                  nsINetworkLinkService,
                  nsIRunnable,
                  nsIObserver)

nsNotifyAddrListener::nsNotifyAddrListener()
    : mLinkUp(true)  // assume true by default
    , mStatusKnown(false)
    , mAllowChangedEvent(true)
{
}

nsNotifyAddrListener::~nsNotifyAddrListener()
{
    NS_ASSERTION(!mThread, "nsNotifyAddrListener thread shutdown failed");

    close(mShutdownPipe[0]);
    close(mShutdownPipe[1]);
}

NS_IMETHODIMP
nsNotifyAddrListener::GetIsLinkUp(bool *aIsUp)
{
    // XXX This function has not yet been implemented for this platform
    *aIsUp = mLinkUp;
    return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::GetLinkStatusKnown(bool *aIsUp)
{
    // XXX This function has not yet been implemented for this platform
    *aIsUp = mStatusKnown;
    return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::GetLinkType(uint32_t *aLinkType)
{
  NS_ENSURE_ARG_POINTER(aLinkType);

  // XXX This function has not yet been implemented for this platform
  *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  return NS_OK;
}

void nsNotifyAddrListener::OnNetlinkMessage(int aNetlinkSocket)
{
    struct  nlmsghdr *nlh;
    struct  rtmsg *route_entry;
    char buffer[4095];

    NS_ASSERTION(aNetlinkSocket >= 0, "bad aNetlinkSocket");

    // Receiving netlink socket data
    ssize_t rc = recv(aNetlinkSocket, buffer, sizeof(buffer), 0);
    if (rc < 0) {
        // LOG this?
        return;
    }
    size_t netlink_bytes = rc;

    nlh = reinterpret_cast<struct nlmsghdr *>(buffer);

    bool networkChange = false;

    for (; NLMSG_OK(nlh, netlink_bytes);
         nlh = NLMSG_NEXT(nlh, netlink_bytes)) {

        if (NLMSG_DONE == nlh->nlmsg_type) {
            break;
        }

        switch(nlh->nlmsg_type) {
        case RTM_DELROUTE:
        case RTM_NEWROUTE:
            // Get the route data
            route_entry = static_cast<struct rtmsg *>(NLMSG_DATA(nlh));

            // We are just intrested in main routing table
            if (route_entry->rtm_table != RT_TABLE_MAIN)
                continue;

            networkChange = true;
            break;

        case RTM_NEWADDR:
            networkChange = true;
            break;

        default:
            continue;
        }
    }

    if (networkChange && mAllowChangedEvent) {
        SendEvent(NS_NETWORK_LINK_DATA_CHANGED);
    }
}

NS_IMETHODIMP
nsNotifyAddrListener::Run()
{
    PR_SetCurrentThreadName("Link Monitor");

    int netlinkSocket = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (netlinkSocket < 0) {
        return NS_ERROR_FAILURE;
    }

    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));   // clear addr

    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_IPV4_ROUTE | RTMGRP_IPV4_IFADDR |
        RTMGRP_IPV6_IFADDR | RTMGRP_IPV6_ROUTE;

    if (bind(netlinkSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        // failure!
        close(netlinkSocket);
        return NS_ERROR_FAILURE;
    }

    // switch the socket into non-blocking
    int flags = fcntl(netlinkSocket, F_GETFL, 0);
    (void)fcntl(netlinkSocket, F_SETFL, flags | O_NONBLOCK);


    struct pollfd fds[2];
    fds[0].fd = mShutdownPipe[0];
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = netlinkSocket;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    nsresult rv = NS_OK;
    bool shutdown = false;
    while (!shutdown) {
        int rc = poll(fds, 2, -1);
        if (rc > 0) {
            if (fds[0].revents & POLLIN) {
                // shutdown, abort the loop!
                shutdown = true;
            } else if (fds[1].revents & POLLIN) {
                OnNetlinkMessage(netlinkSocket);
            }
        } else if (rc < 0) {
            rv = NS_ERROR_FAILURE;
            break;
        }
    }

    close(netlinkSocket);

    return rv;
}

NS_IMETHODIMP
nsNotifyAddrListener::Observe(nsISupports *subject,
                              const char *topic,
                              const char16_t *data)
{
    if (!strcmp("xpcom-shutdown-threads", topic)) {
        Shutdown();
    }

    return NS_OK;
}

nsresult
nsNotifyAddrListener::Init(void)
{
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (!observerService)
        return NS_ERROR_FAILURE;

    nsresult rv = observerService->AddObserver(this, "xpcom-shutdown-threads",
                                               false);
    NS_ENSURE_SUCCESS(rv, rv);

    Preferences::AddBoolVarCache(&mAllowChangedEvent,
                                 NETWORK_NOTIFY_CHANGED_PREF, true);

    rv = NS_NewThread(getter_AddRefs(mThread), this);
    NS_ENSURE_SUCCESS(rv, rv);

    if (-1 == pipe(mShutdownPipe)) {
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

nsresult
nsNotifyAddrListener::Shutdown(void)
{
    // remove xpcom shutdown observer
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService)
        observerService->RemoveObserver(this, "xpcom-shutdown-threads");

    // awake the thread to make it terminate
    write(mShutdownPipe[1], "1", 1);

    nsresult rv = mThread->Shutdown();

    // Have to break the cycle here, otherwise nsNotifyAddrListener holds
    // onto the thread and the thread holds onto the nsNotifyAddrListener
    // via its mRunnable
    mThread = nullptr;

    return rv;
}

/* Sends the given event.  Assumes aEventID never goes out of scope (static
 * strings are ideal).
 */
nsresult
nsNotifyAddrListener::SendEvent(const char *aEventID)
{
    if (!aEventID)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    nsCOMPtr<nsIRunnable> event = new ChangeEvent(this, aEventID);
    if (NS_FAILED(rv = NS_DispatchToMainThread(event)))
        NS_WARNING("Failed to dispatch ChangeEvent");
    return rv;
}

NS_IMETHODIMP
nsNotifyAddrListener::ChangeEvent::Run()
{
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService)
        observerService->NotifyObservers(
                mService, NS_NETWORK_LINK_TOPIC,
                NS_ConvertASCIItoUTF16(mEventID).get());
    return NS_OK;
}
