/* -*- Mode: c++; c-basic-offset: 2; tab-width: 2; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoViewOutputStream.h"
#include "mozilla/fallible.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(GeckoViewOutputStream, nsIOutputStream);

NS_IMETHODIMP
GeckoViewOutputStream::Close() {
  mStream->SendEof();
  return NS_OK;
}

NS_IMETHODIMP
GeckoViewOutputStream::Flush() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
GeckoViewOutputStream::StreamStatus() {
  return mStream->IsStreamClosed() ? NS_BASE_STREAM_CLOSED : NS_OK;
}

NS_IMETHODIMP
GeckoViewOutputStream::Write(const char* buf, uint32_t count,
                             uint32_t* retval) {
  jni::ByteArray::LocalRef buffer = jni::ByteArray::New(
      reinterpret_cast<const int8_t*>(buf), count, fallible);
  if (!buffer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (NS_FAILED(mStream->AppendBuffer(buffer))) {
    // The stream was closed, abort reading this channel.
    return NS_BASE_STREAM_CLOSED;
  }
  // Return amount of bytes written
  *retval = count;

  return NS_OK;
}

NS_IMETHODIMP
GeckoViewOutputStream::WriteFrom(nsIInputStream* fromStream, uint32_t count,
                                 uint32_t* retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GeckoViewOutputStream::WriteSegments(nsReadSegmentFun reader, void* closure,
                                     uint32_t count, uint32_t* retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GeckoViewOutputStream::IsNonBlocking(bool* retval) {
  *retval = true;
  return NS_OK;
}
