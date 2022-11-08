/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportStreamProxy.h"

#include "WebTransportLog.h"
#include "Http3WebTransportStream.h"
#include "nsProxyRelease.h"
#include "nsSocketTransportService2.h"

namespace mozilla::net {

NS_IMPL_ADDREF(WebTransportStreamProxy)
NS_IMPL_RELEASE(WebTransportStreamProxy)

NS_INTERFACE_MAP_BEGIN(WebTransportStreamProxy)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebTransportReceiveStream)
  NS_INTERFACE_MAP_ENTRY(nsIWebTransportReceiveStream)
  NS_INTERFACE_MAP_ENTRY(nsIWebTransportSendStream)
  NS_INTERFACE_MAP_ENTRY(nsIWebTransportBidirectionalStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncOutputStream)
NS_INTERFACE_MAP_END

WebTransportStreamProxy::WebTransportStreamProxy(
    Http3WebTransportStream* aStream)
    : mWebTransportStream(aStream) {}

WebTransportStreamProxy::~WebTransportStreamProxy() {
  // mWebTransportStream needs to be destroyed on the socket thread.
  NS_ProxyRelease("WebTransportStreamProxy::~WebTransportStreamProxy",
                  gSocketTransportService, mWebTransportStream.forget());
}

NS_IMETHODIMP WebTransportStreamProxy::SendStopSending(uint8_t aError) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::SendFin(void) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::Reset(uint8_t aErrorCode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::Close() {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::Available(uint64_t* aAvailable) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::Read(char* aBuf, uint32_t aCount,
                                            uint32_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::ReadSegments(nsWriteSegmentFun aWriter,
                                                    void* aClosure,
                                                    uint32_t aCount,
                                                    uint32_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::IsNonBlocking(bool* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::CloseWithStatus(nsresult aStatus) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncWait(
    nsIInputStreamCallback* aCallback, uint32_t aFlags,
    uint32_t aRequestedCount, nsIEventTarget* aEventTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::Flush() {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::Write(const char* aBuf, uint32_t aCount,
                                             uint32_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

// static
nsresult WebTransportStreamProxy::WriteFromSegments(
    nsIInputStream* input, void* closure, const char* fromSegment,
    uint32_t offset, uint32_t count, uint32_t* countRead) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::WriteFrom(nsIInputStream* aFromStream,
                                                 uint32_t aCount,
                                                 uint32_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::WriteSegments(nsReadSegmentFun aReader,
                                                     void* aClosure,
                                                     uint32_t aCount,
                                                     uint32_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncWait(
    nsIOutputStreamCallback* aCallback, uint32_t aFlags,
    uint32_t aRequestedCount, nsIEventTarget* aEventTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace mozilla::net
