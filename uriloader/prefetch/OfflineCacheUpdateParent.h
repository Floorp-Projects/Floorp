/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOfflineCacheUpdateParent_h
#define nsOfflineCacheUpdateParent_h

#include "mozilla/docshell/POfflineCacheUpdateParent.h"
#include "nsIOfflineCacheUpdate.h"

#include "nsString.h"

namespace mozilla {
namespace docshell {

class OfflineCacheUpdateParent : public POfflineCacheUpdateParent
                               , public nsIOfflineCacheUpdateObserver
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOFFLINECACHEUPDATEOBSERVER

    nsresult
    Schedule(const URI& manifestURI,
             const URI& documentURI,
             const nsCString& clientID,
             const bool& stickDocument);

    OfflineCacheUpdateParent();
    ~OfflineCacheUpdateParent();

    virtual void ActorDestroy(ActorDestroyReason why);

private:
    void RefcountHitZero();
    bool mIPCClosed;
};

}
}

#endif
