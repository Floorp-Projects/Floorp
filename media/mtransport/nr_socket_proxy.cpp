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

#include "nr_socket_proxy.h"

#include "mozilla/ErrorNames.h"

#include "WebrtcProxyChannelWrapper.h"

namespace mozilla {
using namespace net;

using std::shared_ptr;

class NrSocketProxyData {
 public:
  explicit NrSocketProxyData(nsTArray<uint8_t>&& aData) : mData(aData) {
    MOZ_COUNT_CTOR(NrSocketProxyData);
  }

  ~NrSocketProxyData() { MOZ_COUNT_DTOR(NrSocketProxyData); }

  const nsTArray<uint8_t>& GetData() const { return mData; }

 private:
  nsTArray<uint8_t> mData;
};

NrSocketProxy::NrSocketProxy(const shared_ptr<NrSocketProxyConfig>& aConfig)
    : mClosed(false),
      mReadOffset(0),
      mConfig(aConfig),
      mWebrtcProxyChannel(nullptr) {
  r_log(LOG_GENERIC, LOG_DEBUG, "NrSocketProxy::NrSocketProxy %p\n", this);
  MOZ_ASSERT(mConfig, "config should not be null");
}

NrSocketProxy::~NrSocketProxy() {
  r_log(LOG_GENERIC, LOG_DEBUG, "NrSocketProxy::~NrSocketProxy %p\n", this);
  MOZ_ASSERT(!mWebrtcProxyChannel, "webrtc proxy channel not null");
}

int NrSocketProxy::create(nr_transport_addr* aAddr) {
  int32_t port;
  nsCString host;

  // Sanity check
  if (nr_transport_addr_get_addrstring_and_port(aAddr, &host, &port)) {
    return R_FAILED;
  }

  if (nr_transport_addr_copy(&my_addr_, aAddr)) {
    return R_FAILED;
  }

  return 0;
}

int NrSocketProxy::connect(nr_transport_addr* aAddr) {
  r_log(LOG_GENERIC, LOG_DEBUG, "NrSocketProxy::Connect %p\n", this);

  nsCString host;
  int port;

  if (nr_transport_addr_get_addrstring_and_port(aAddr, &host, &port)) {
    return R_FAILED;
  }

  mWebrtcProxyChannel = new WebrtcProxyChannelWrapper(this);

  mWebrtcProxyChannel->AsyncOpen(host, port, mConfig);

  // trigger nr_socket_buffered to set write/read callback
  return R_WOULDBLOCK;
}

void NrSocketProxy::close() {
  r_log(LOG_GENERIC, LOG_DEBUG, "NrSocketProxy::Close %p\n", this);

  if (mClosed) {
    return;
  }

  mClosed = true;

  // We're not always open at this point.
  if (mWebrtcProxyChannel) {
    mWebrtcProxyChannel->Close();
    mWebrtcProxyChannel = nullptr;
  }
}

int NrSocketProxy::write(const void* aBuffer, size_t aCount, size_t* aWrote) {
  r_log(LOG_GENERIC, LOG_DEBUG, "NrSocketProxy::Write %p count=%zu\n", this,
        aCount);

  if (mClosed) {
    return R_FAILED;
  }

  if (!aWrote) {
    return R_FAILED;
  }

  *aWrote = aCount;

  if (aCount > 0) {
    nsTArray<uint8_t> writeData;
    writeData.SetLength(aCount);
    memcpy(writeData.Elements(), aBuffer, aCount);

    mWebrtcProxyChannel->SendWrite(std::move(writeData));
  }

  return 0;
}

int NrSocketProxy::read(void* aBuffer, size_t aCount, size_t* aRead) {
  r_log(LOG_GENERIC, LOG_DEBUG, "NrSocketProxy::Read %p\n", this);

  if (mClosed) {
    return R_FAILED;
  }

  if (!aRead) {
    return R_FAILED;
  }

  *aRead = 0;

  if (mReadQueue.empty()) {
    return R_WOULDBLOCK;
  }

  while (aCount > 0 && !mReadQueue.empty()) {
    const NrSocketProxyData& data = mReadQueue.front();

    size_t remainingCount = data.GetData().Length() - mReadOffset;
    size_t amountToCopy = std::min(aCount, remainingCount);

    char* buffer = static_cast<char*>(aBuffer) + (*aRead);

    memcpy(buffer, data.GetData().Elements() + mReadOffset, amountToCopy);

    mReadOffset += amountToCopy;
    *aRead += amountToCopy;
    aCount -= amountToCopy;

    if (remainingCount == amountToCopy) {
      mReadOffset = 0;
      mReadQueue.pop_front();
    }
  }

  return 0;
}

int NrSocketProxy::getaddr(nr_transport_addr* aAddr) {
  r_log(LOG_GENERIC, LOG_DEBUG, "NrSocketProxy::GetAddr %p\n", this);
  return nr_transport_addr_copy(aAddr, &my_addr_);
}

int NrSocketProxy::sendto(const void* aBuffer, size_t aCount, int aFlags,
                          nr_transport_addr* aAddr) {
  // never call this
  MOZ_ASSERT(0);
  return R_FAILED;
}

int NrSocketProxy::recvfrom(void* aBuffer, size_t aCount, size_t* aRead,
                            int aFlags, nr_transport_addr* aAddr) {
  // never call this
  MOZ_ASSERT(0);
  return R_FAILED;
}

int NrSocketProxy::listen(int aBacklog) { return R_INTERNAL; }

int NrSocketProxy::accept(nr_transport_addr* aAddr, nr_socket** aSocket) {
  return R_INTERNAL;
}

// WebrtcProxyChannelCallback
void NrSocketProxy::OnClose(nsresult aReason) {
  nsCString errorName;
  GetErrorName(aReason, errorName);

  r_log(LOG_GENERIC, LOG_ERR, "NrSocketProxy::OnClose %p reason=%u name=%s\n",
        this, static_cast<uint32_t>(aReason), errorName.get());

  close();

  DoCallbacks();
}

void NrSocketProxy::OnConnected() {
  r_log(LOG_GENERIC, LOG_DEBUG, "NrSocketProxy::OnConnected %p\n", this);

  DoCallbacks();
}

void NrSocketProxy::OnRead(nsTArray<uint8_t>&& aReadData) {
  r_log(LOG_GENERIC, LOG_DEBUG, "NrSocketProxy::OnRead %p read=%zu\n", this,
        aReadData.Length());

  mReadQueue.emplace_back(std::move(aReadData));

  DoCallbacks();
}

void NrSocketProxy::DoCallbacks() {
  size_t lastCount = -1;
  size_t currentCount = 0;
  while ((poll_flags() & PR_POLL_READ) != 0 &&
         // Make sure whatever is reading knows we're closed. This doesn't need
         // to happen for writes since ICE doesn't like a failing writable.
         (mClosed || (currentCount = CountUnreadBytes()) > 0) &&
         lastCount != currentCount) {
    fire_callback(NR_ASYNC_WAIT_READ);
    lastCount = currentCount;
  }

  // We're always ready to write after we're connected. The parent process will
  // buffer writes for us.
  if (!mClosed && mWebrtcProxyChannel && (poll_flags() & PR_POLL_WRITE) != 0) {
    fire_callback(NR_ASYNC_WAIT_WRITE);
  }
}

size_t NrSocketProxy::CountUnreadBytes() const {
  size_t count = 0;

  for (const NrSocketProxyData& data : mReadQueue) {
    count += data.GetData().Length();
  }

  MOZ_ASSERT(count >= mReadOffset, "offset exceeds read buffer length");

  count -= mReadOffset;

  return count;
}

void NrSocketProxy::AssignChannel_DoNotUse(
    WebrtcProxyChannelWrapper* aWrapper) {
  mWebrtcProxyChannel = aWrapper;
}

}  // namespace mozilla
