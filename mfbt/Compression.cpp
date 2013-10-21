/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Compression.h"
#include "mozilla/CheckedInt.h"
using namespace mozilla::Compression;

namespace {

#include "lz4.c"

}/* anonymous namespace */

/* Our wrappers */

size_t
LZ4::compress(const char* source, size_t inputSize, char* dest)
{
    CheckedInt<int> inputSizeChecked = inputSize;
    MOZ_ASSERT(inputSizeChecked.isValid());
    return LZ4_compress(source, dest, inputSizeChecked.value());
}

size_t
LZ4::compressLimitedOutput(const char* source, size_t inputSize, char* dest, size_t maxOutputSize)
{
    CheckedInt<int> inputSizeChecked = inputSize;
    MOZ_ASSERT(inputSizeChecked.isValid());
    CheckedInt<int> maxOutputSizeChecked = maxOutputSize;
    MOZ_ASSERT(maxOutputSizeChecked.isValid());
    return LZ4_compress_limitedOutput(source, dest, inputSizeChecked.value(),
                                      maxOutputSizeChecked.value());
}

bool
LZ4::decompress(const char* source, char* dest, size_t outputSize)
{
    CheckedInt<int> outputSizeChecked = outputSize;
    MOZ_ASSERT(outputSizeChecked.isValid());
    int ret = LZ4_decompress_fast(source, dest, outputSizeChecked.value());
    return ret >= 0;
}

bool
LZ4::decompress(const char* source, size_t inputSize, char* dest, size_t maxOutputSize,
                size_t *outputSize)
{
    CheckedInt<int> maxOutputSizeChecked = maxOutputSize;
    MOZ_ASSERT(maxOutputSizeChecked.isValid());
    CheckedInt<int> inputSizeChecked = inputSize;
    MOZ_ASSERT(inputSizeChecked.isValid());

    int ret = LZ4_decompress_safe(source, dest, inputSizeChecked.value(),
                                  maxOutputSizeChecked.value());
    if (ret >= 0) {
        *outputSize = ret;
        return true;
    } else {
        *outputSize = 0;
        return false;
    }
}

