/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInputStreamChannel_h__
#define nsInputStreamChannel_h__

#include "nsBaseChannel.h"
#include "nsIInputStreamChannel.h"

//-----------------------------------------------------------------------------

namespace mozilla {
namespace net {

class nsInputStreamChannel : public nsBaseChannel
                           , public nsIInputStreamChannel
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAMCHANNEL

    nsInputStreamChannel() :
      mIsSrcdocChannel(false) {}

protected:
    virtual ~nsInputStreamChannel() = default;

    virtual nsresult OpenContentStream(bool async, nsIInputStream **result,
                                       nsIChannel** channel) override;

    virtual void OnChannelDone() override {
        mContentStream = nullptr;
    }

private:
    nsCOMPtr<nsIInputStream> mContentStream;
    nsCOMPtr<nsIURI> mBaseURI;
    nsString mSrcdocData;
    bool mIsSrcdocChannel;
};

} // namespace net
} // namespace mozilla

#endif // !nsInputStreamChannel_h__
