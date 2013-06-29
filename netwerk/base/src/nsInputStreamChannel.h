/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInputStreamChannel_h__
#define nsInputStreamChannel_h__

#include "nsBaseChannel.h"
#include "nsIInputStreamChannel.h"

//-----------------------------------------------------------------------------

class nsInputStreamChannel : public nsBaseChannel
                           , public nsIInputStreamChannel
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAMCHANNEL

    nsInputStreamChannel() :
      mIsSrcdocChannel(false) {}

protected:
    virtual ~nsInputStreamChannel() {}

    virtual nsresult OpenContentStream(bool async, nsIInputStream **result,
                                       nsIChannel** channel);

private:
    nsCOMPtr<nsIInputStream> mContentStream;
    nsString mSrcdocData;
    bool mIsSrcdocChannel;
};

#endif // !nsInputStreamChannel_h__
