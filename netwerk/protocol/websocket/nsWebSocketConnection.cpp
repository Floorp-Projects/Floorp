/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWebSocketConnection.h"

#include "WebSocketLog.h"
#include "nsIOService.h"
#include "nsISocketTransport.h"

NS_IMPL_ISUPPORTS(nsWebSocketConnection, nsIWebSocketConnection,
                  nsIInputStreamCallback, nsIOutputStreamCallback)

nsWebSocketConnection::nsWebSocketConnection(
    nsISocketTransport* aTransport, nsIAsyncInputStream* aInputStream,
    nsIAsyncOutputStream* aOutputStream)
    : mTransport(aTransport),
      mSocketIn(aInputStream),
      mSocketOut(aOutputStream),
      mWriteOffset(0),
      mStartReadingCalled(false) {}

NS_IMETHODIMP
nsWebSocketConnection::Init(nsIWebSocketConnectionListener* aListener,
                            nsIEventTarget* aEventTarget) {
  NS_ENSURE_ARG_POINTER(aListener);
  NS_ENSURE_ARG_POINTER(aEventTarget);

  mListener = aListener;
  mEventTarget = aEventTarget;

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

NS_IMETHODIMP
nsWebSocketConnection::Close() {
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

  mListener = nullptr;
  return NS_OK;
}

nsresult nsWebSocketConnection::EnqueueOutputData(
    nsTArray<uint8_t>&& aHeader, nsTArray<uint8_t>&& aPayload) {
  MOZ_ASSERT(mEventTarget->IsOnCurrentThread());

  mOutputQueue.emplace_back(std::move(aHeader));
  mOutputQueue.emplace_back(std::move(aPayload));

  if (mSocketOut) {
    mSocketOut->AsyncWait(this, 0, 0, mEventTarget);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebSocketConnection::EnqueueOutputData(const uint8_t* aHdrBuf,
                                         uint32_t aHdrBufLength,
                                         const uint8_t* aPayloadBuf,
                                         uint32_t aPayloadBufLength) {
  LOG(("nsWebSocketConnection::EnqueueOutputData %p\n", this));

  nsTArray<uint8_t> header;
  header.AppendElements(aHdrBuf, aHdrBufLength);
  nsTArray<uint8_t> payload;
  payload.AppendElements(aPayloadBuf, aPayloadBufLength);
  return EnqueueOutputData(std::move(header), std::move(payload));
}

NS_IMETHODIMP
nsWebSocketConnection::StartReading() {
  if (!mSocketIn) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(!mStartReadingCalled, "StartReading twice");
  mStartReadingCalled = true;
  return mSocketIn->AsyncWait(this, 0, 0, mEventTarget);
}

NS_IMETHODIMP
nsWebSocketConnection::DrainSocketData() {
  MOZ_ASSERT(mEventTarget->IsOnCurrentThread());

  if (!mSocketIn || !mListener) {
    return NS_ERROR_NOT_AVAILABLE;
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

  return NS_OK;
}

NS_IMETHODIMP
nsWebSocketConnection::GetSecurityInfo(nsISupports** aSecurityInfo) {
  LOG(("nsWebSocketConnection::GetSecurityInfo() %p\n", this));
  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

  if (mTransport) {
    if (NS_FAILED(mTransport->GetSecurityInfo(aSecurityInfo)))
      *aSecurityInfo = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocketConnection::OnInputStreamReady(nsIAsyncInputStream* aStream) {
  LOG(("nsWebSocketConnection::OnInputStreamReady() %p\n", this));
  MOZ_ASSERT(mEventTarget->IsOnCurrentThread());

  if (!mSocketIn)  // did we we clean up the socket after scheduling InputReady?
    return NS_OK;

  if (!mListener) return NS_OK;

  // this is after the http upgrade - so we are speaking websockets
  uint8_t buffer[2048];
  uint32_t count;
  nsresult rv;

  do {
    rv = mSocketIn->Read((char*)buffer, 2048, &count);
    LOG(("nsWebSocketConnection::OnInputStreamReady: read %u rv %" PRIx32 "\n",
         count, static_cast<uint32_t>(rv)));

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      mSocketIn->AsyncWait(this, 0, 0, mEventTarget);
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
nsWebSocketConnection::OnOutputStreamReady(nsIAsyncOutputStream* aStream) {
  LOG(("nsWebSocketConnection::OnOutputStreamReady() %p\n", this));
  MOZ_ASSERT(mEventTarget->IsOnCurrentThread());

  if (!mListener) return NS_OK;

  while (!mOutputQueue.empty()) {
    const OutputData& data = mOutputQueue.front();

    char* buffer = reinterpret_cast<char*>(
                       const_cast<uint8_t*>(data.GetData().Elements())) +
                   mWriteOffset;
    uint32_t toWrite = data.GetData().Length() - mWriteOffset;

    uint32_t wrote = 0;
    nsresult rv = mSocketOut->Write(buffer, toWrite, &wrote);
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      mSocketOut->AsyncWait(this, 0, 0, mEventTarget);
      return NS_OK;
    }

    if (NS_FAILED(rv)) {
      LOG(("nsWebSocketConnection::OnOutputStreamReady %p failed %u\n", this,
           static_cast<uint32_t>(rv)));
      mListener->OnError(rv);
      return NS_OK;
    }

    mWriteOffset += wrote;

    if (toWrite == wrote) {
      mWriteOffset = 0;
      mOutputQueue.pop_front();
    }
  }

  return NS_OK;
}
