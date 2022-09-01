/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIInterfaceRequestor.h"
#include "WebSocketConnection.h"

#include "WebSocketLog.h"
#include "mozilla/net/WebSocketConnectionListener.h"
#include "nsIOService.h"
#include "nsISSLSocketControl.h"
#include "nsISocketTransport.h"
#include "nsITransportSecurityInfo.h"
#include "nsSocketTransportService2.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(WebSocketConnection, nsIInputStreamCallback,
                  nsIOutputStreamCallback)

WebSocketConnection::WebSocketConnection(nsISocketTransport* aTransport,
                                         nsIAsyncInputStream* aInputStream,
                                         nsIAsyncOutputStream* aOutputStream)
    : mTransport(aTransport),
      mSocketIn(aInputStream),
      mSocketOut(aOutputStream) {
  LOG(("WebSocketConnection ctor %p\n", this));
}

WebSocketConnection::~WebSocketConnection() {
  LOG(("WebSocketConnection dtor %p\n", this));
}

nsresult WebSocketConnection::Init(WebSocketConnectionListener* aListener) {
  NS_ENSURE_ARG_POINTER(aListener);

  mListener = aListener;
  nsresult rv;
  mSocketThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mTransport) {
    return NS_ERROR_FAILURE;
  }

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks = do_QueryInterface(mListener);
    mTransport->SetSecurityCallbacks(callbacks);
  } else {
    // NOTE: we don't use security callbacks in socket process.
    mTransport->SetSecurityCallbacks(nullptr);
  }
  return mTransport->SetEventSink(nullptr, nullptr);
}

void WebSocketConnection::GetIoTarget(nsIEventTarget** aTarget) {
  nsCOMPtr<nsIEventTarget> target = mSocketThread;
  return target.forget(aTarget);
}

void WebSocketConnection::Close() {
  LOG(("WebSocketConnection::Close %p\n", this));
  MOZ_ASSERT(OnSocketThread());

  if (mTransport) {
    mTransport->SetSecurityCallbacks(nullptr);
    mTransport->SetEventSink(nullptr, nullptr);
    mTransport->Close(NS_BASE_STREAM_CLOSED);
    mTransport = nullptr;
  }

  if (mSocketIn) {
    if (mStartReadingCalled) {
      mSocketIn->AsyncWait(nullptr, 0, 0, nullptr);
    }
    mSocketIn = nullptr;
  }

  if (mSocketOut) {
    mSocketOut->AsyncWait(nullptr, 0, 0, nullptr);
    mSocketOut = nullptr;
  }
}

nsresult WebSocketConnection::WriteOutputData(nsTArray<uint8_t>&& aData) {
  MOZ_ASSERT(OnSocketThread());

  if (!mSocketOut) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mOutputQueue.emplace_back(std::move(aData));
  return OnOutputStreamReady(mSocketOut);
}

nsresult WebSocketConnection::WriteOutputData(const uint8_t* aHdrBuf,
                                              uint32_t aHdrBufLength,
                                              const uint8_t* aPayloadBuf,
                                              uint32_t aPayloadBufLength) {
  MOZ_ASSERT_UNREACHABLE("Should not be called");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult WebSocketConnection::StartReading() {
  MOZ_ASSERT(OnSocketThread());

  if (!mSocketIn) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(!mStartReadingCalled, "StartReading twice");
  mStartReadingCalled = true;
  return mSocketIn->AsyncWait(this, 0, 0, mSocketThread);
}

void WebSocketConnection::DrainSocketData() {
  MOZ_ASSERT(OnSocketThread());

  if (!mSocketIn || !mListener) {
    return;
  }

  // If we leave any data unconsumed (including the tcp fin) a RST will be
  // generated The right thing to do here is shutdown(SHUT_WR) and then wait a
  // little while to see if any data comes in.. but there is no reason to delay
  // things for that when the websocket handshake is supposed to guarantee a
  // quiet connection except for that fin.
  char buffer[512];
  uint32_t count = 0;
  uint32_t total = 0;
  nsresult rv;
  do {
    total += count;
    rv = mSocketIn->Read(buffer, 512, &count);
    if (rv != NS_BASE_STREAM_WOULD_BLOCK && (NS_FAILED(rv) || count == 0)) {
      mListener->OnTCPClosed();
    }
  } while (NS_SUCCEEDED(rv) && count > 0 && total < 32000);
}

nsresult WebSocketConnection::GetSecurityInfo(
    nsITransportSecurityInfo** aSecurityInfo) {
  LOG(("WebSocketConnection::GetSecurityInfo() %p\n", this));
  MOZ_ASSERT(OnSocketThread());
  *aSecurityInfo = nullptr;

  if (mTransport) {
    nsCOMPtr<nsISSLSocketControl> tlsSocketControl;
    nsresult rv =
        mTransport->GetTlsSocketControl(getter_AddRefs(tlsSocketControl));
    if (NS_FAILED(rv)) {
      return rv;
    }
    nsCOMPtr<nsITransportSecurityInfo> securityInfo(
        do_QueryInterface(tlsSocketControl));
    if (securityInfo) {
      securityInfo.forget(aSecurityInfo);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnection::OnInputStreamReady(nsIAsyncInputStream* aStream) {
  LOG(("WebSocketConnection::OnInputStreamReady() %p\n", this));
  MOZ_ASSERT(OnSocketThread());
  MOZ_ASSERT(mListener);

  // did we we clean up the socket after scheduling InputReady?
  if (!mSocketIn) {
    return NS_OK;
  }

  // this is after the http upgrade - so we are speaking websockets
  uint8_t buffer[2048];
  uint32_t count;
  nsresult rv;

  do {
    rv = mSocketIn->Read((char*)buffer, 2048, &count);
    LOG(("WebSocketConnection::OnInputStreamReady: read %u rv %" PRIx32 "\n",
         count, static_cast<uint32_t>(rv)));

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      mSocketIn->AsyncWait(this, 0, 0, mSocketThread);
      return NS_OK;
    }

    if (NS_FAILED(rv)) {
      mListener->OnError(rv);
      return rv;
    }

    if (count == 0) {
      mListener->OnError(NS_BASE_STREAM_CLOSED);
      return NS_OK;
    }

    rv = mListener->OnDataReceived(buffer, count);
    if (NS_FAILED(rv)) {
      mListener->OnError(rv);
      return rv;
    }
  } while (NS_SUCCEEDED(rv) && mSocketIn && mListener);

  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnection::OnOutputStreamReady(nsIAsyncOutputStream* aStream) {
  LOG(("WebSocketConnection::OnOutputStreamReady() %p\n", this));
  MOZ_ASSERT(OnSocketThread());
  MOZ_ASSERT(mListener);

  if (!mSocketOut) {
    return NS_OK;
  }

  while (!mOutputQueue.empty()) {
    const OutputData& data = mOutputQueue.front();

    char* buffer = reinterpret_cast<char*>(
                       const_cast<uint8_t*>(data.GetData().Elements())) +
                   mWriteOffset;
    uint32_t toWrite = data.GetData().Length() - mWriteOffset;

    uint32_t wrote = 0;
    nsresult rv = mSocketOut->Write(buffer, toWrite, &wrote);
    LOG(("WebSocketConnection::OnOutputStreamReady: write %u rv %" PRIx32,
         wrote, static_cast<uint32_t>(rv)));

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      mSocketOut->AsyncWait(this, 0, 0, mSocketThread);
      return rv;
    }

    if (NS_FAILED(rv)) {
      LOG(("WebSocketConnection::OnOutputStreamReady %p failed %u\n", this,
           static_cast<uint32_t>(rv)));
      mListener->OnError(rv);
      return NS_OK;
    }

    mWriteOffset += wrote;

    if (toWrite == wrote) {
      mWriteOffset = 0;
      mOutputQueue.pop_front();
    } else {
      mSocketOut->AsyncWait(this, 0, 0, mSocketThread);
    }
  }

  return NS_OK;
}

}  // namespace mozilla::net
