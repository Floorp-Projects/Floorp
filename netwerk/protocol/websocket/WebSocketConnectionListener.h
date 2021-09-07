/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketConnectionListener_h
#define mozilla_net_WebSocketConnectionListener_h

namespace mozilla {
namespace net {

class WebSocketConnectionListener : public nsISupports {
 public:
  virtual void OnError(nsresult aStatus) = 0;
  virtual void OnTCPClosed() = 0;
  virtual nsresult OnDataReceived(uint8_t* aData, uint32_t aCount) = 0;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebSocketConnectionListener_h
