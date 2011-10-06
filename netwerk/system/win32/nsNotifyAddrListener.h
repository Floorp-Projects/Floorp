/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et sw=4 ts=4: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is written by Juan Lang.
 *
 * The Initial Developer of the Original Code is 
 * Juan Lang.
 * Portions created by the Initial Developer are Copyright (C) 2003,2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSINETWORKLINKSERVICE
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSIOBSERVER

    nsNotifyAddrListener();
    virtual ~nsNotifyAddrListener();

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

    DWORD GetOperationalStatus(DWORD aAdapterIndex);
    DWORD CheckIPAddrTable(void);
    DWORD CheckAdaptersInfo(void);
    DWORD CheckAdaptersAddresses(void);
    BOOL  CheckIsGateway(PIP_ADAPTER_ADDRESSES aAdapter);
    BOOL  CheckICSStatus(PWCHAR aAdapterName);
    void  CheckLinkStatus(void);

    nsCOMPtr<nsIThread> mThread;

    OSVERSIONINFO mOSVerInfo;
    HANDLE        mShutdownEvent;
};

#endif /* NSNOTIFYADDRLISTENER_H_ */
