/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "mozilla/dom/OwnedRustBuffer.h"

namespace mozilla::uniffi {

using dom::ArrayBuffer;

OwnedRustBuffer::OwnedRustBuffer(const RustBuffer& aBuf) {
  mBuf = aBuf;
  MOZ_ASSERT(IsValid());
}

Result<OwnedRustBuffer, nsCString> OwnedRustBuffer::FromArrayBuffer(
    const ArrayBuffer& aArrayBuffer) {
  return aArrayBuffer.ProcessData(
      [](const Span<uint8_t>& aData,
         JS::AutoCheckCannotGC&&) -> Result<OwnedRustBuffer, nsCString> {
        if (aData.Length() > INT32_MAX) {
          return Err("Input ArrayBuffer is too large"_ns);
        }

        RustCallStatus status{};
        RustBuffer buf = uniffi_rustbuffer_alloc(
            static_cast<int32_t>(aData.Length()), &status);
        buf.len = aData.Length();
        if (status.code != 0) {
          if (status.error_buf.data) {
            auto message = nsCString("uniffi_rustbuffer_alloc: ");
            message.Append(nsDependentCSubstring(
                reinterpret_cast<char*>(status.error_buf.data),
                status.error_buf.len));
            RustCallStatus status2{};
            uniffi_rustbuffer_free(status.error_buf, &status2);
            MOZ_RELEASE_ASSERT(status2.code == 0,
                               "Freeing a rustbuffer should never fail");
            return Err(message);
          }

          return Err("Unknown error allocating rust buffer"_ns);
        }

        memcpy(buf.data, aData.Elements(), buf.len);
        return OwnedRustBuffer(buf);
      });
}

OwnedRustBuffer::OwnedRustBuffer(OwnedRustBuffer&& aOther) : mBuf(aOther.mBuf) {
  aOther.mBuf = RustBuffer{0};
}

OwnedRustBuffer& OwnedRustBuffer::operator=(OwnedRustBuffer&& aOther) {
  if (&aOther != this) {
    FreeData();
  }
  mBuf = aOther.mBuf;
  aOther.mBuf = RustBuffer{0};
  return *this;
}

void OwnedRustBuffer::FreeData() {
  if (IsValid()) {
    RustCallStatus status{};
    uniffi_rustbuffer_free(mBuf, &status);
    MOZ_RELEASE_ASSERT(status.code == 0,
                       "Freeing a rustbuffer should never fail");
    mBuf = {0};
  }
}

OwnedRustBuffer::~OwnedRustBuffer() { FreeData(); }

RustBuffer OwnedRustBuffer::IntoRustBuffer() {
  RustBuffer rv = mBuf;
  mBuf = {};
  return rv;
}

JSObject* OwnedRustBuffer::IntoArrayBuffer(JSContext* cx) {
  JS::Rooted<JSObject*> obj(cx);
  {
    int32_t len = mBuf.len;
    void* data = mBuf.data;
    auto userData = MakeUnique<OwnedRustBuffer>(std::move(*this));
    UniquePtr<void, JS::BufferContentsDeleter> dataPtr{
        data, {&ArrayBufferFreeFunc, userData.release()}};
    obj = JS::NewExternalArrayBuffer(cx, len, std::move(dataPtr));
  }
  return obj;
}

void OwnedRustBuffer::ArrayBufferFreeFunc(void* contents, void* userData) {
  UniquePtr<OwnedRustBuffer> buf{reinterpret_cast<OwnedRustBuffer*>(userData)};
}
}  // namespace mozilla::uniffi
