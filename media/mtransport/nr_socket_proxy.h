/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Copyright (c) 2007, Adobe Systems, Incorporated
Copyright (c) 2013, Mozilla

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance, Mozilla nor
 the names of its contributors may be used to endorse or promote
 products derived from this software without specific prior written
 permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef nr_socket_proxy_h__
#define nr_socket_proxy_h__

#include <list>

#include "mozilla/net/WebrtcProxyChannelCallback.h"

#include "nsTArray.h"

extern "C" {
#include "nr_api.h"
#include "nr_socket.h"
#include "transport_addr.h"
}

#include "nr_socket_prsock.h"

namespace mozilla {
using namespace net;

namespace net {
class WebrtcProxyChannelWrapper;
}  // namespace net

class NrSocketProxyData;
class NrSocketProxyConfig;

class NrSocketProxy : public NrSocketBase, public WebrtcProxyChannelCallback {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NrSocketProxy, override)

  explicit NrSocketProxy(const std::shared_ptr<NrSocketProxyConfig>& aConfig);

  // NrSocketBase
  int create(nr_transport_addr* aAddr) override;
  int connect(nr_transport_addr* aAddr) override;
  void close() override;
  int write(const void* aBuffer, size_t aCount, size_t* aWrote) override;
  int read(void* aBuffer, size_t aCount, size_t* aRead) override;
  int getaddr(nr_transport_addr* aAddr) override;
  int sendto(const void* aBuffer, size_t aCount, int aFlags,
             nr_transport_addr* aAddr) override;
  int recvfrom(void* aBuffer, size_t aCount, size_t* aRead, int aFlags,
               nr_transport_addr* aAddr) override;
  int listen(int aBacklog) override;
  int accept(nr_transport_addr* aAddr, nr_socket** aSocket) override;

  bool IsProxied() const override { return true; }

  // WebrtcProxyChannelCallback
  void OnClose(nsresult aReason) override;
  void OnConnected() override;
  void OnRead(nsTArray<uint8_t>&& aReadData) override;

  size_t CountUnreadBytes() const;

  // for gtests
  void AssignChannel_DoNotUse(WebrtcProxyChannelWrapper* aWrapper);

 protected:
  virtual ~NrSocketProxy();

 private:
  void DoCallbacks();

  bool mClosed;

  size_t mReadOffset;
  std::list<NrSocketProxyData> mReadQueue;

  std::shared_ptr<NrSocketProxyConfig> mConfig;

  RefPtr<WebrtcProxyChannelWrapper> mWebrtcProxyChannel;
};

}  // namespace mozilla

#endif  // nr_socket_proxy_h__
