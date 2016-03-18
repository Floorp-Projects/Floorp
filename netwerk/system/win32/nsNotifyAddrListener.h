/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et sw=4 ts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSNOTIFYADDRLISTENER_H_
#define NSNOTIFYADDRLISTENER_H_

#include <windows.h>
#include <winsock2.h>
#include <iptypes.h>
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
    void CheckLinkStatus(void);

protected:
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

    bool mLinkUp;
    bool mStatusKnown;
    bool mCheckAttempted;

    nsresult Shutdown(void);
    nsresult SendEvent(const char *aEventID);

    DWORD CheckAdaptersAddresses(void);

    // Checks for an Internet Connection Sharing (ICS) gateway.
    bool  CheckICSGateway(PIP_ADAPTER_ADDRESSES aAdapter);
    bool  CheckICSStatus(PWCHAR aAdapterName);

    nsCOMPtr<nsIThread> mThread;

private:
    // Returns the new timeout period for coalescing (or INFINITE)
    DWORD nextCoalesceWaitTime();

    // Called for every detected network change
    nsresult NetworkChanged();

    HANDLE mCheckEvent;

    // set true when mCheckEvent means shutdown
    bool mShutdown;

    // This is a checksum of various meta data for all network interfaces
    // considered UP at last check.
    ULONG mIPInterfaceChecksum;

    // start time of the checking
    mozilla::TimeStamp mStartTime;

    // Network changed events are enabled
    bool mAllowChangedEvent;

    // Flag set while coalescing change events
    bool mCoalescingActive;

    // Time stamp for first event during coalescing
    mozilla::TimeStamp mChangeTime;
};

#endif /* NSNOTIFYADDRLISTENER_H_ */
