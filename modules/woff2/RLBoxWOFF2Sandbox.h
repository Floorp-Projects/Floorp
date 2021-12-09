/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MODULES_WOFF2_RLBOX_WOFF2_SANDBOX_H_
#define MODULES_WOFF2_RLBOX_WOFF2_SANDBOX_H_

#include <brotli/decode.h>

extern "C" {

// Because RLBox doesn't (yet) cleanly support {size,uint8}_t types on Windows,
// we're using unsigned long instead of size_t and char instead of uint8_t.

// Since RLBox doesn't support C++ APIs, we expose C wrappers for the WOFF2.
bool RLBoxConvertWOFF2ToTTF(const char* aData, unsigned long aLength,
                            unsigned long aDecompressedSize,
                            unsigned long* aResultSize, void** aResultOwningStr,
                            char** aResultData);
// RLBoxDeleteWOFF2String is used to delete the C++ string allocated by
// RLBoxConvertWOFF2ToTTF.
void RLBoxDeleteWOFF2String(void** aStr);

typedef BrotliDecoderResult(BrotliDecompressCallback)(
    unsigned long aEncodedSize, const char* aEncodedBuffer,
    unsigned long* aDecodedSize, char* aDecodedBuffer);

// Callback to the unsandboxed Brotli.

extern BrotliDecompressCallback* sRLBoxBrotliDecompressCallback;

void RegisterWOFF2Callback(BrotliDecompressCallback* aCallback);
BrotliDecoderResult RLBoxBrotliDecoderDecompress(size_t aEncodedSize,
                                                 const uint8_t* aEncodedBuffer,
                                                 size_t* aDecodedSize,
                                                 uint8_t* aDecodedBuffer);
};

#endif
