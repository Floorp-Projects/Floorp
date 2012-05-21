/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceChannel_h_
#define nsDeviceChannel_h_

#include "nsBaseChannel.h"

class nsDeviceChannel : public nsBaseChannel
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsDeviceChannel();
  ~nsDeviceChannel();

  nsresult Init(nsIURI* uri);
  nsresult OpenContentStream(bool aAsync,
                             nsIInputStream **aStream,
                             nsIChannel **aChannel);
};
#endif
