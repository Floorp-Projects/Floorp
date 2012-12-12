/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOfflineCacheUpdateParent_h
#define nsOfflineCacheUpdateParent_h

#include "mozilla/docshell/POfflineCacheUpdateParent.h"
#include "nsIOfflineCacheUpdate.h"

#include "nsString.h"
#include "nsILoadContext.h"

namespace mozilla {

namespace dom {
class TabParent;
}

namespace ipc {
class URIParams;
} // namespace ipc

namespace docshell {

class OfflineCacheUpdateParent : public POfflineCacheUpdateParent
                               , public nsIOfflineCacheUpdateObserver
                               , public nsILoadContext
{
    typedef mozilla::ipc::URIParams URIParams;

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOFFLINECACHEUPDATEOBSERVER
    NS_DECL_NSILOADCONTEXT

    nsresult
    Schedule(const URIParams& manifestURI,
             const URIParams& documentURI,
             const bool& stickDocument);

    OfflineCacheUpdateParent(uint32_t aAppId, bool aIsInBrowser);
    ~OfflineCacheUpdateParent();

    virtual void ActorDestroy(ActorDestroyReason why);

private:
    void RefcountHitZero();
    bool mIPCClosed;

    bool     mIsInBrowserElement;
    uint32_t mAppId;
};

} // namespace docshell
} // namespace mozilla

#endif
