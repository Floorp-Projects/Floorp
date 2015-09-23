/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOfflineCacheUpdateParent_h
#define nsOfflineCacheUpdateParent_h

#include "mozilla/docshell/POfflineCacheUpdateParent.h"
#include "mozilla/BasePrincipal.h"
#include "nsIOfflineCacheUpdate.h"

#include "nsString.h"
#include "nsILoadContext.h"

namespace mozilla {

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

    void
    StopSendingMessagesToChild()
    {
      mIPCClosed = true;
    }

    OfflineCacheUpdateParent(uint32_t aAppId, bool aIsInBrowser);

    virtual void ActorDestroy(ActorDestroyReason aWhy) override;
private:
    ~OfflineCacheUpdateParent();

    bool mIPCClosed;

    mozilla::OriginAttributes mOriginAttributes;
};

} // namespace docshell
} // namespace mozilla

#endif
