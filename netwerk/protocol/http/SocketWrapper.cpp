/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "SocketWrapper.h"
#include "TLSFilterTransaction.h"

namespace mozilla {
namespace net {

SocketInWrapper::SocketInWrapper(nsIAsyncInputStream* aWrapped,
                                 TLSFilterTransaction* aFilter)
    : mStream(aWrapped), mTLSFilter(aFilter) {}

nsresult SocketInWrapper::OnWriteSegment(char* segment, uint32_t count,
                                         uint32_t* countWritten) {
  LOG(("SocketInWrapper OnWriteSegment %d %p filter=%p\n", count, this,
       mTLSFilter.get()));

  nsresult rv = mStream->Read(segment, count, countWritten);
  LOG(("SocketInWrapper OnWriteSegment %p wrapped read %" PRIx32 " %d\n", this,
       static_cast<uint32_t>(rv), *countWritten));
  return rv;
}

NS_IMETHODIMP
SocketInWrapper::Read(char* aBuf, uint32_t aCount, uint32_t* _retval) {
  LOG(("SocketInWrapper Read %d %p filter=%p\n", aCount, this,
       mTLSFilter.get()));

  if (!mTLSFilter) {
    return NS_ERROR_UNEXPECTED;  // protect potentially dangling mTLSFilter
  }

  // mTLSFilter->mSegmentWriter MUST be this at ctor time
  return mTLSFilter->OnWriteSegment(aBuf, aCount, _retval);
}

SocketOutWrapper::SocketOutWrapper(nsIAsyncOutputStream* aWrapped,
                                   TLSFilterTransaction* aFilter)
    : mStream(aWrapped), mTLSFilter(aFilter) {}

nsresult SocketOutWrapper::OnReadSegment(const char* segment, uint32_t count,
                                         uint32_t* countWritten) {
  return mStream->Write(segment, count, countWritten);
}

NS_IMETHODIMP
SocketOutWrapper::Write(const char* aBuf, uint32_t aCount, uint32_t* _retval) {
  LOG(("SocketOutWrapper Write %d %p filter=%p\n", aCount, this,
       mTLSFilter.get()));

  // mTLSFilter->mSegmentReader MUST be this at ctor time
  if (!mTLSFilter) {
    return NS_ERROR_UNEXPECTED;  // protect potentially dangling mTLSFilter
  }

  return mTLSFilter->OnReadSegment(aBuf, aCount, _retval);
}

NS_IMPL_ISUPPORTS(SocketInWrapper, nsIAsyncInputStream)
NS_IMPL_ISUPPORTS(SocketOutWrapper, nsIAsyncOutputStream)

}  // namespace net
}  // namespace mozilla
