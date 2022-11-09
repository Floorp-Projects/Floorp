/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebTransportStreamProxy_h
#define mozilla_net_WebTransportStreamProxy_h

#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIWebTransportStream.h"
#include "nsCOMPtr.h"

namespace mozilla::net {

class Http3WebTransportStream;

class WebTransportStreamProxy final : public nsIWebTransportReceiveStream,
                                      public nsIWebTransportSendStream,
                                      public nsIWebTransportBidirectionalStream,
                                      public nsIAsyncInputStream,
                                      public nsIAsyncOutputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit WebTransportStreamProxy(Http3WebTransportStream* aStream);

  NS_IMETHOD SendStopSending(uint8_t aError) override;
  NS_IMETHOD SendFin() override;
  NS_IMETHOD Reset(uint8_t aErrorCode) override;
  NS_IMETHOD GetSendStreamStats(
      nsIWebTransportSendStreamStatsCallback* aCallback) override;

  NS_IMETHOD Close() override;
  NS_IMETHOD Available(uint64_t* aAvailable) override;
  NS_IMETHOD Read(char* aBuf, uint32_t aCount, uint32_t* aResult) override;
  NS_IMETHOD ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                          uint32_t aCount, uint32_t* aResult) override;
  NS_IMETHOD IsNonBlocking(bool* aResult) override;
  NS_IMETHOD CloseWithStatus(nsresult aStatus) override;
  NS_IMETHOD AsyncWait(nsIInputStreamCallback* aCallback, uint32_t aFlags,
                       uint32_t aRequestedCount,
                       nsIEventTarget* aEventTarget) override;

  NS_IMETHOD Flush() override;
  NS_IMETHOD Write(const char* aBuf, uint32_t aCount,
                   uint32_t* aResult) override;
  NS_IMETHOD WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                       uint32_t* aResult) override;
  NS_IMETHOD WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                           uint32_t aCount, uint32_t* aResult) override;
  NS_IMETHOD AsyncWait(nsIOutputStreamCallback* aCallback, uint32_t aFlags,
                       uint32_t aRequestedCount,
                       nsIEventTarget* aEventTarget) override;

 private:
  virtual ~WebTransportStreamProxy();
  void EnsureWriter();

  static nsresult WriteFromSegments(nsIInputStream*, void*, const char*,
                                    uint32_t offset, uint32_t count,
                                    uint32_t* countRead);

  RefPtr<Http3WebTransportStream> mWebTransportStream;
  nsCOMPtr<nsIAsyncOutputStream> mWriter;
};

}  // namespace mozilla::net

#endif
