/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et sw=4 ts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSNOTIFYADDRLISTENER_LINUX_H_
#define NSNOTIFYADDRLISTENER_LINUX_H_

#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "nsINetworkLinkService.h"
#include "nsIRunnable.h"
#include "nsIObserver.h"
#include "nsThreadUtils.h"
#include "nsCOMPtr.h"
#include "mozilla/TimeStamp.h"

class nsNotifyAddrListener : public nsINetworkLinkService,
                             public nsIRunnable,
                             public nsIObserver
{
    virtual ~nsNotifyAddrListener();

public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSINETWORKLINKSERVICE
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSIOBSERVER

    nsNotifyAddrListener();
    nsresult Init(void);

private:
    class ChangeEvent : public nsRunnable {
    public:
        NS_DECL_NSIRUNNABLE
        ChangeEvent(nsINetworkLinkService *aService, const char *aEventID)
            : mService(aService), mEventID(aEventID) {
        }
    private:
        nsCOMPtr<nsINetworkLinkService> mService;
        const char *mEventID;
    };

    // Called when xpcom-shutdown-threads is received.
    nsresult Shutdown(void);

    // Sends the network event.
    nsresult SendEvent(const char *aEventID);

    // Deals with incoming NETLINK messages.
    void OnNetlinkMessage(int NetlinkSocket);

    nsCOMPtr<nsIThread> mThread;

    // The network is up.
    bool mLinkUp;

    // The network's up/down status is known.
    bool mStatusKnown;

    // A pipe to signal shutdown with.
    int mShutdownPipe[2];

    // Network changed events are enabled
    bool mAllowChangedEvent;
};

#endif /* NSNOTIFYADDRLISTENER_LINUX_H_ */
