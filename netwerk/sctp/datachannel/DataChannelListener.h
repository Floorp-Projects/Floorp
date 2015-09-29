/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NETWERK_SCTP_DATACHANNEL_DATACHANNELLISTENER_H_
#define NETWERK_SCTP_DATACHANNEL_DATACHANNELLISTENER_H_

#include "nsISupports.h"
#include "nsString.h"

namespace mozilla {

// Implemented by consumers of a Channel to receive messages.
// Can't nest it in DataChannelConnection because C++ doesn't allow forward
// refs to embedded classes
class DataChannelListener {
public:
  virtual ~DataChannelListener() {}

  // Called when a DOMString message is received.
  virtual nsresult OnMessageAvailable(nsISupports *aContext,
                                      const nsACString& message) = 0;

  // Called when a binary message is received.
  virtual nsresult OnBinaryMessageAvailable(nsISupports *aContext,
                                            const nsACString& message) = 0;

  // Called when the channel is connected
  virtual nsresult OnChannelConnected(nsISupports *aContext) = 0;

  // Called when the channel is closed
  virtual nsresult OnChannelClosed(nsISupports *aContext) = 0;

  // Called when the BufferedAmount drops below the BufferedAmountLowThreshold
  virtual nsresult OnBufferLow(nsISupports *aContext) = 0;
};

}

#endif
