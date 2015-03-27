/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSyncStreamListener_h__
#define nsSyncStreamListener_h__

#include "nsISyncStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

//-----------------------------------------------------------------------------

class nsSyncStreamListener final : public nsISyncStreamListener
                                 , public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISYNCSTREAMLISTENER
    NS_DECL_NSIINPUTSTREAM

    nsSyncStreamListener()
        : mStatus(NS_OK)
        , mKeepWaiting(false)
        , mDone(false) {}

    nsresult Init();

private:
    ~nsSyncStreamListener() {}

    nsresult WaitForData();

    nsCOMPtr<nsIInputStream>    mPipeIn;
    nsCOMPtr<nsIOutputStream>   mPipeOut;
    nsresult                    mStatus;
    bool                        mKeepWaiting;
    bool                        mDone;
};

#endif // nsSyncStreamListener_h__
