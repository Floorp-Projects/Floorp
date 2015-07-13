/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 sts=4 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_DATACHANNELCHILD_H
#define NS_DATACHANNELCHILD_H

#include "nsDataChannel.h"
#include "nsIChildChannel.h"
#include "nsISupportsImpl.h"

#include "mozilla/net/PDataChannelChild.h"

namespace mozilla {
namespace net {

class DataChannelChild : public nsDataChannel
                       , public nsIChildChannel
                       , public PDataChannelChild
{
public:
    explicit DataChannelChild(nsIURI *uri);

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSICHILDCHANNEL

protected:
    virtual void ActorDestroy(ActorDestroyReason why) override;

private:
    ~DataChannelChild();

    void AddIPDLReference();

    bool mIPCOpen;
};

} // namespace net
} // namespace mozilla

#endif /* NS_DATACHANNELCHILD_H */
