/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <woff2/decode.h>
#include <cassert>
#include "RLBoxWOFF2Sandbox.h"

bool RLBoxConvertWOFF2ToTTF(const char* aData, unsigned long aLength,
                            unsigned long aDecompressedSize,
                            unsigned long* aResultSize, void** aResultOwningStr,
                            char** aResultData) {
  std::unique_ptr<std::string> buf =
      std::make_unique<std::string>(aDecompressedSize, 0);
  woff2::WOFF2StringOut out(buf.get());
  if (!woff2::ConvertWOFF2ToTTF(reinterpret_cast<const uint8_t*>(aData),
                                aLength, &out)) {
    return false;
  }
  *aResultSize = out.Size();
  // Return the string and its underlying C string. We need both to make sure we
  // can free the string (which we do with RLBoxDeleteWOFF2String).
  *aResultData = buf->data();
  *aResultOwningStr = static_cast<void*>(buf.release());
  return true;
}

void RLBoxDeleteWOFF2String(void** aStr) {
  std::string* buf = static_cast<std::string*>(*aStr);
  delete buf;
}

BrotliDecompressCallback* sRLBoxBrotliDecompressCallback = nullptr;

void RegisterWOFF2Callback(BrotliDecompressCallback* aCallback) {
#ifdef MOZ_IN_WASM_SANDBOX
  // When Woff2 is wasmboxed, we need to register a callback for brotli
  // decompression. The easiest way to store this is in a static variable. This
  // is thread-safe because each (potentially-concurrent) woff2 instance gets
  // its own sandbox with its own copy of the statics.
  //
  // When the sandbox is disabled (replaced with the noop sandbox), setting the
  // callback is actually racey. However, we don't actually need a callback in
  // that case, and can just invoke brotli directly.
  sRLBoxBrotliDecompressCallback = aCallback;
#endif
}

BrotliDecoderResult RLBoxBrotliDecoderDecompress(size_t aEncodedSize,
                                                 const uint8_t* aEncodedBuffer,
                                                 size_t* aDecodedSize,
                                                 uint8_t* aDecodedBuffer) {
#ifdef MOZ_IN_WASM_SANDBOX
  assert(sRLBoxBrotliDecompressCallback);
  return sRLBoxBrotliDecompressCallback(
      aEncodedSize, reinterpret_cast<const char*>(aEncodedBuffer), aDecodedSize,
      reinterpret_cast<char*>(aDecodedBuffer));
#else
  return BrotliDecoderDecompress(aEncodedSize, aEncodedBuffer, aDecodedSize,
                                 aDecodedBuffer);
#endif
}
