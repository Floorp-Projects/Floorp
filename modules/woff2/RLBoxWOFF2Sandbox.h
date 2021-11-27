/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MODULES_WOFF2_RLBOX_WOFF2_SANDBOX_H_
#define MODULES_WOFF2_RLBOX_WOFF2_SANDBOX_H_

#include <brotli/decode.h>

extern "C" {

// Since RLBox doesn't support C++ APIs, we expose C wrappers for the WOFF2.
size_t RLBoxComputeWOFF2FinalSize(const uint8_t* aData, size_t aLength);
bool RLBoxConvertWOFF2ToTTF(const uint8_t* aData, size_t aLength,
                            size_t aDecompressedSize, size_t* aResultSize,
                            void** aResultOwningStr, uint8_t** aResultData);
// RLBoxDeleteWOFF2String is used to delete the C++ string allocated by
// RLBoxConvertWOFF2ToTTF.
void RLBoxDeleteWOFF2String(void** aStr);

// Type of brotli decoder function. Because RLBox doesn't (yet) cleanly support
// {size,uint8}_t types for callbacks, we're using unsigned long instead of
// size_t and char instead of uint8_t.

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
