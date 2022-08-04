/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketWrapper_h
#define mozilla_net_SocketWrapper_h

#include "nsAHttpTransaction.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"

namespace mozilla {
namespace net {

class TLSFilterTransaction;

class SocketInWrapper : public nsIAsyncInputStream,
                        public nsAHttpSegmentWriter {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_NSIASYNCINPUTSTREAM(mStream->)

  SocketInWrapper(nsIAsyncInputStream* aWrapped, TLSFilterTransaction* aFilter);

  NS_IMETHOD Close() override {
    mTLSFilter = nullptr;
    return mStream->Close();
  }

  NS_IMETHOD Available(uint64_t* _retval) override {
    return mStream->Available(_retval);
  }

  NS_IMETHOD IsNonBlocking(bool* _retval) override {
    return mStream->IsNonBlocking(_retval);
  }

  NS_IMETHOD ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                          uint32_t aCount, uint32_t* _retval) override {
    return mStream->ReadSegments(aWriter, aClosure, aCount, _retval);
  }

  // finally, ones that don't get forwarded :)
  NS_IMETHOD Read(char* aBuf, uint32_t aCount, uint32_t* _retval) override;
  virtual nsresult OnWriteSegment(char* segment, uint32_t count,
                                  uint32_t* countWritten) override;

 private:
  virtual ~SocketInWrapper() = default;
  ;

  nsCOMPtr<nsIAsyncInputStream> mStream;
  RefPtr<TLSFilterTransaction> mTLSFilter;
};

class SocketOutWrapper : public nsIAsyncOutputStream,
                         public nsAHttpSegmentReader {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_NSIASYNCOUTPUTSTREAM(mStream->)

  SocketOutWrapper(nsIAsyncOutputStream* aWrapped,
                   TLSFilterTransaction* aFilter);

  NS_IMETHOD Close() override {
    mTLSFilter = nullptr;
    return mStream->Close();
  }

  NS_IMETHOD Flush() override { return mStream->Flush(); }

  NS_IMETHOD IsNonBlocking(bool* _retval) override {
    return mStream->IsNonBlocking(_retval);
  }

  NS_IMETHOD WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                           uint32_t aCount, uint32_t* _retval) override {
    return mStream->WriteSegments(aReader, aClosure, aCount, _retval);
  }

  NS_IMETHOD WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                       uint32_t* _retval) override {
    return mStream->WriteFrom(aFromStream, aCount, _retval);
  }

  // finally, ones that don't get forwarded :)
  NS_IMETHOD Write(const char* aBuf, uint32_t aCount,
                   uint32_t* _retval) override;
  virtual nsresult OnReadSegment(const char* segment, uint32_t count,
                                 uint32_t* countWritten) override;

 private:
  virtual ~SocketOutWrapper() = default;
  ;

  nsCOMPtr<nsIAsyncOutputStream> mStream;
  RefPtr<TLSFilterTransaction> mTLSFilter;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketWrapper_h
