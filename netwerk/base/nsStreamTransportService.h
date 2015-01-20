/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIStreamTransportService.h"
#include "nsIEventTarget.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"

class nsIThreadPool;

class nsStreamTransportService MOZ_FINAL : public nsIStreamTransportService
                                         , public nsIEventTarget
                                         , public nsIObserver
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISTREAMTRANSPORTSERVICE
    NS_DECL_NSIEVENTTARGET
    NS_DECL_NSIOBSERVER

    nsresult Init();

    nsStreamTransportService() : mShutdownLock("nsStreamTransportService.mShutdownLock"),
                                 mIsShutdown(false) {}

private:
    ~nsStreamTransportService();

    nsCOMPtr<nsIThreadPool> mPool;

    mozilla::Mutex mShutdownLock;
    bool mIsShutdown;
};
