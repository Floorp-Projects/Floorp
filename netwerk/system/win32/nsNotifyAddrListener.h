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
    nsresult SendEventToUI(const char *aEventID);

    DWORD CheckAdaptersAddresses(void);
    bool  CheckIsGateway(PIP_ADAPTER_ADDRESSES aAdapter);
    bool  CheckICSStatus(PWCHAR aAdapterName);
    void  CheckLinkStatus(void);

    nsCOMPtr<nsIThread> mThread;

    HANDLE        mShutdownEvent;
};

#endif /* NSNOTIFYADDRLISTENER_H_ */
