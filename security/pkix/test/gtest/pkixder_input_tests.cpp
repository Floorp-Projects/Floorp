/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <functional>
#include <vector>
#include <gtest/gtest.h>

#include "pkix/bind.h"
#include "pkixder.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::der;

namespace {

class pkixder_input_tests : public ::testing::Test { };

static const uint8_t DER_SEQUENCE_EMPTY[] = {
  0x30,                       // SEQUENCE
  0x00,                       // length
};

static const uint8_t DER_SEQUENCE_NOT_EMPTY[] = {
  0x30,                       // SEQUENCE
  0x01,                       // length
  'X',                        // value
};

static const uint8_t DER_SEQUENCE_NOT_EMPTY_VALUE[] = {
  'X',                        // value
};

static const uint8_t DER_SEQUENCE_NOT_EMPTY_VALUE_TRUNCATED[] = {
  0x30,                       // SEQUENCE
  0x01,                       // length
};

const uint8_t DER_SEQUENCE_OF_INT8[] = {
  0x30,                       // SEQUENCE
  0x09,                       // length
  0x02, 0x01, 0x01,           // INTEGER length 1 value 0x01
  0x02, 0x01, 0x02,           // INTEGER length 1 value 0x02
  0x02, 0x01, 0x03            // INTEGER length 1 value 0x03
};

const uint8_t DER_TRUNCATED_SEQUENCE_OF_INT8[] = {
  0x30,                       // SEQUENCE
  0x09,                       // length
  0x02, 0x01, 0x01,           // INTEGER length 1 value 0x01
  0x02, 0x01, 0x02            // INTEGER length 1 value 0x02
  // MISSING DATA HERE ON PURPOSE
};

const uint8_t DER_OVERRUN_SEQUENCE_OF_INT8[] = {
  0x30,                       // SEQUENCE
  0x09,                       // length
  0x02, 0x01, 0x01,           // INTEGER length 1 value 0x01
  0x02, 0x01, 0x02,           // INTEGER length 1 value 0x02
  0x02, 0x02, 0xFF, 0x03      // INTEGER length 2 value 0xFF03
};

const uint8_t DER_INT16[] = {
  0x02,                       // INTEGER
  0x02,                       // length
  0x12, 0x34                  // 0x1234
};

TEST_F(pkixder_input_tests, InputInit)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));
}

TEST_F(pkixder_input_tests, InputInitWithNullPointerOrZeroLength)
{
  Input input;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Init(nullptr, 0));

  ASSERT_EQ(Result::ERROR_BAD_DER, input.Init(nullptr, 100));

  // Though it seems odd to initialize with zero-length and non-null ptr, this
  // is working as intended. The Input class was intended to protect against
  // buffer overflows, and there's no risk with the current behavior. See bug
  // 1000354.
  ASSERT_EQ(Success, input.Init((const uint8_t*) "hello", 0));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, InputInitWithLargeData)
{
  Input input;
  // Data argument length does not matter, it is not touched, just
  // needs to be non-null
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Init((const uint8_t*) "", 0xffff+1));

  ASSERT_EQ(Success, input.Init((const uint8_t*) "", 0xffff));
}

TEST_F(pkixder_input_tests, InputInitMultipleTimes)
{
  Input input;

  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));

  ASSERT_EQ(Result::FATAL_ERROR_INVALID_ARGS,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));
}

TEST_F(pkixder_input_tests, ExpectSuccess)
{
  Input input;

  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));
  ASSERT_EQ(Success,
            input.Expect(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, ExpectMismatch)
{
  Input input;

  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));

  const uint8_t expected[] = { 0x11, 0x22 };
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Expect(expected, sizeof expected));
}

TEST_F(pkixder_input_tests, ExpectTooMuch)
{
  Input input;

  const uint8_t der[] = { 0x11, 0x22 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  const uint8_t expected[] = { 0x11, 0x22, 0x33 };
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Expect(expected, sizeof expected));
}

TEST_F(pkixder_input_tests, PeekWithinBounds)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x11 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));
  ASSERT_TRUE(input.Peek(0x11));
  ASSERT_FALSE(input.Peek(0x22));
}

TEST_F(pkixder_input_tests, PeekPastBounds)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22 };
  ASSERT_EQ(Success, input.Init(der, 1));

  uint8_t readByte;
  ASSERT_EQ(Success, input.Read(readByte));
  ASSERT_EQ(0x11, readByte);
  ASSERT_FALSE(input.Peek(0x22));
}

TEST_F(pkixder_input_tests, ReadByte)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  uint8_t readByte1;
  ASSERT_EQ(Success, input.Read(readByte1));
  ASSERT_EQ(0x11, readByte1);

  uint8_t readByte2;
  ASSERT_EQ(Success, input.Read(readByte2));
  ASSERT_EQ(0x22, readByte2);
}

TEST_F(pkixder_input_tests, ReadBytePastEnd)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22 };
  // Initialize with too-short length
  ASSERT_EQ(Success, input.Init(der, 1));

  uint8_t readByte1 = 0;
  ASSERT_EQ(Success, input.Read(readByte1));
  ASSERT_EQ(0x11, readByte1);

  uint8_t readByte2 = 0;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Read(readByte2));
  ASSERT_NE(0x22, readByte2);
}

TEST_F(pkixder_input_tests, ReadByteWrapAroundPointer)
{
  // The original implementation of our buffer read overflow checks was
  // susceptible to integer overflows which could make the checks ineffective.
  // This attempts to verify that we've fixed that. Unfortunately, decrementing
  // a null pointer is undefined behavior according to the C++ language spec.,
  // but this should catch the problem on at least some compilers, if not all of
  // them.
  const uint8_t* der = nullptr;
  --der;
  Input input;
  ASSERT_EQ(Success, input.Init(der, 0));
  uint8_t b;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Read(b));
}

TEST_F(pkixder_input_tests, ReadWord)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  uint16_t readWord1 = 0;
  ASSERT_EQ(Success, input.Read(readWord1));
  ASSERT_EQ(0x1122, readWord1);

  uint16_t readWord2 = 0;
  ASSERT_EQ(Success, input.Read(readWord2));
  ASSERT_EQ(0x3344, readWord2);
}

TEST_F(pkixder_input_tests, ReadWordPastEnd)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  // Initialize with too-short length
  ASSERT_EQ(Success, input.Init(der, 2));

  uint16_t readWord1 = 0;
  ASSERT_EQ(Success, input.Read(readWord1));
  ASSERT_EQ(0x1122, readWord1);

  uint16_t readWord2 = 0;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Read(readWord2));
  ASSERT_NE(0x3344, readWord2);
}

TEST_F(pkixder_input_tests, ReadWordWithInsufficentData)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22 };
  ASSERT_EQ(Success, input.Init(der, 1));

  uint16_t readWord1 = 0;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Read(readWord1));
  ASSERT_NE(0x1122, readWord1);
}

TEST_F(pkixder_input_tests, ReadWordWrapAroundPointer)
{
  // The original implementation of our buffer read overflow checks was
  // susceptible to integer overflows which could make the checks ineffective.
  // This attempts to verify that we've fixed that. Unfortunately, decrementing
  // a null pointer is undefined behavior according to the C++ language spec.,
  // but this should catch the problem on at least some compilers, if not all of
  // them.
  const uint8_t* der = nullptr;
  --der;
  Input input;
  ASSERT_EQ(Success, input.Init(der, 0));
  uint16_t b;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Read(b));
}

TEST_F(pkixder_input_tests, InputSkip)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  ASSERT_EQ(Success, input.Skip(1));

  uint8_t readByte1 = 0;
  ASSERT_EQ(Success, input.Read(readByte1));
  ASSERT_EQ(0x22, readByte1);

  ASSERT_EQ(Success, input.Skip(1));

  uint8_t readByte2 = 0;
  ASSERT_EQ(Success, input.Read(readByte2));
  ASSERT_EQ(0x44, readByte2);
}

TEST_F(pkixder_input_tests, InputSkipToEnd)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));
  ASSERT_EQ(Success, input.Skip(sizeof der));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, InputSkipPastEnd)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  ASSERT_EQ(Result::ERROR_BAD_DER, input.Skip(sizeof der + 1));
}

TEST_F(pkixder_input_tests, InputSkipToNewInput)
{
  Input input;
  const uint8_t der[] = { 0x01, 0x02, 0x03, 0x04 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  Input skippedInput;
  ASSERT_EQ(Success, input.Skip(3, skippedInput));

  uint8_t readByte1 = 0;
  ASSERT_EQ(Success, input.Read(readByte1));
  ASSERT_EQ(0x04, readByte1);

  ASSERT_TRUE(input.AtEnd());

  // Input has no Remaining() or Length() so we simply read the bytes
  // and then expect to be at the end.

  for (uint8_t i = 1; i <= 3; ++i) {
    uint8_t readByte = 0;
    ASSERT_EQ(Success, skippedInput.Read(readByte));
    ASSERT_EQ(i, readByte);
  }

  ASSERT_TRUE(skippedInput.AtEnd());
}

TEST_F(pkixder_input_tests, InputSkipToNewInputPastEnd)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  Input skippedInput;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Skip(sizeof der * 2, skippedInput));
}

TEST_F(pkixder_input_tests, InputSkipToSECItem)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  const uint8_t expectedItemData[] = { 0x11, 0x22, 0x33 };

  SECItem item;
  ASSERT_EQ(Success, input.Skip(sizeof expectedItemData, item));
  ASSERT_EQ(siBuffer, item.type);
  ASSERT_EQ(sizeof expectedItemData, item.len);
  ASSERT_EQ(der, item.data);
  ASSERT_EQ(0, memcmp(item.data, expectedItemData, sizeof expectedItemData));
}

TEST_F(pkixder_input_tests, SkipWrapAroundPointer)
{
  // The original implementation of our buffer read overflow checks was
  // susceptible to integer overflows which could make the checks ineffective.
  // This attempts to verify that we've fixed that. Unfortunately, decrementing
  // a null pointer is undefined behavior according to the C++ language spec.,
  // but this should catch the problem on at least some compilers, if not all of
  // them.
  const uint8_t* der = nullptr;
  --der;
  Input input;
  ASSERT_EQ(Success, input.Init(der, 0));
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Skip(1));
}

TEST_F(pkixder_input_tests, SkipToSECItemPastEnd)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  SECItem skippedSECItem;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Skip(sizeof der + 1, skippedSECItem));
}

TEST_F(pkixder_input_tests, ExpectTagAndSkipValue)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));

  ASSERT_EQ(Success, ExpectTagAndSkipValue(input, SEQUENCE));
  ASSERT_EQ(Success, End(input));
}

TEST_F(pkixder_input_tests, ExpectTagAndSkipValueWithTruncatedData)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_TRUNCATED_SEQUENCE_OF_INT8,
                                sizeof DER_TRUNCATED_SEQUENCE_OF_INT8));

  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndSkipValue(input, SEQUENCE));
}

TEST_F(pkixder_input_tests, ExpectTagAndSkipValueWithOverrunData)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_OVERRUN_SEQUENCE_OF_INT8,
                                sizeof DER_OVERRUN_SEQUENCE_OF_INT8));
  ASSERT_EQ(Success, ExpectTagAndSkipValue(input, SEQUENCE));
  ASSERT_EQ(Result::ERROR_BAD_DER, End(input));
}

TEST_F(pkixder_input_tests, AtEndOnUnInitializedInput)
{
  Input input;
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, AtEndAtBeginning)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));
  ASSERT_FALSE(input.AtEnd());
}

TEST_F(pkixder_input_tests, AtEndAtEnd)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));
  ASSERT_EQ(Success, input.Skip(sizeof der));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, MarkAndGetSECItem)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  Input::Mark mark = input.GetMark();

  const uint8_t expectedItemData[] = { 0x11, 0x22, 0x33 };

  ASSERT_EQ(Success, input.Skip(sizeof expectedItemData));

  SECItem item;
  memset(&item, 0x00, sizeof item);

  ASSERT_EQ(Success, input.GetSECItem(siBuffer, mark, item));
  ASSERT_EQ(siBuffer, item.type);
  ASSERT_EQ(sizeof expectedItemData, item.len);
  ASSERT_TRUE(item.data);
  ASSERT_EQ(0, memcmp(item.data, expectedItemData, sizeof expectedItemData));
}

// Cannot run this test on debug builds because of the PR_NOT_REACHED
#ifndef DEBUG
TEST_F(pkixder_input_tests, MarkAndGetSECItemDifferentInput)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  Input another;
  Input::Mark mark = another.GetMark();

  ASSERT_EQ(Success, input.Skip(3));

  SECItem item;
  ASSERT_EQ(Result::FATAL_ERROR_INVALID_ARGS,
            input.GetSECItem(siBuffer, mark, item));
}
#endif

TEST_F(pkixder_input_tests, ExpectTagAndLength)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));

  ASSERT_EQ(Success, ExpectTagAndLength(input, SEQUENCE,
                                        sizeof DER_SEQUENCE_OF_INT8 - 2));
}

TEST_F(pkixder_input_tests, ExpectTagAndLengthWithWrongLength)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_INT16, sizeof DER_INT16));

  // Wrong length
  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndLength(input, INTEGER, 4));
}

TEST_F(pkixder_input_tests, ExpectTagAndLengthWithWrongTag)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_INT16, sizeof DER_INT16));

  // Wrong type
  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndLength(input, OCTET_STRING, 2));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLength)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));

  uint16_t length = 0;
  ASSERT_EQ(Success,
            der::internal::ExpectTagAndGetLength(input, SEQUENCE, length));
  ASSERT_EQ(sizeof DER_SEQUENCE_OF_INT8 - 2, length);
  ASSERT_EQ(Success, input.Skip(length));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLengthWithWrongTag)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));

  uint16_t length = 0;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            der::internal::ExpectTagAndGetLength(input, INTEGER, length));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLengthWithWrongLength)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_TRUNCATED_SEQUENCE_OF_INT8,
                                sizeof DER_TRUNCATED_SEQUENCE_OF_INT8));

  uint16_t length = 0;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            der::internal::ExpectTagAndGetLength(input, SEQUENCE, length));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_Input_ValidEmpty)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_EMPTY, sizeof DER_SEQUENCE_EMPTY));
  Input value;
  ASSERT_EQ(Success, ExpectTagAndGetValue(input, SEQUENCE, value));
  ASSERT_TRUE(value.AtEnd());
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_Input_ValidNotEmpty)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_NOT_EMPTY, sizeof DER_SEQUENCE_NOT_EMPTY));
  Input value;
  ASSERT_EQ(Success, ExpectTagAndGetValue(input, SEQUENCE, value));
  ASSERT_TRUE(value.MatchRest(DER_SEQUENCE_NOT_EMPTY_VALUE));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests,
       ExpectTagAndGetValue_Input_InvalidNotEmptyValueTruncated)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_NOT_EMPTY_VALUE_TRUNCATED,
                       sizeof DER_SEQUENCE_NOT_EMPTY_VALUE_TRUNCATED));
  Input value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, SEQUENCE, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_Input_InvalidWrongLength)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_TRUNCATED_SEQUENCE_OF_INT8,
                                sizeof DER_TRUNCATED_SEQUENCE_OF_INT8));
  Input value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, SEQUENCE, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLength_Input_InvalidWrongTag)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_NOT_EMPTY, sizeof DER_SEQUENCE_NOT_EMPTY));
  Input value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, INTEGER, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_SECItem_ValidEmpty)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_EMPTY, sizeof DER_SEQUENCE_EMPTY));
  SECItem value = { siBuffer, nullptr, 5 };
  ASSERT_EQ(Success, ExpectTagAndGetValue(input, SEQUENCE, value));
  ASSERT_EQ(0u, value.len);
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_SECItem_ValidNotEmpty)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_NOT_EMPTY, sizeof DER_SEQUENCE_NOT_EMPTY));
  SECItem value;
  ASSERT_EQ(Success, ExpectTagAndGetValue(input, SEQUENCE, value));
  ASSERT_EQ(sizeof(DER_SEQUENCE_NOT_EMPTY_VALUE), value.len);
  ASSERT_TRUE(value.data);
  ASSERT_FALSE(memcmp(value.data, DER_SEQUENCE_NOT_EMPTY_VALUE,
                      sizeof(DER_SEQUENCE_NOT_EMPTY_VALUE)));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests,
       ExpectTagAndGetValue_SECItem_InvalidNotEmptyValueTruncated)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_NOT_EMPTY_VALUE_TRUNCATED,
                       sizeof DER_SEQUENCE_NOT_EMPTY_VALUE_TRUNCATED));
  SECItem value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, SEQUENCE, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_SECItem_InvalidWrongLength)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_TRUNCATED_SEQUENCE_OF_INT8,
                                sizeof DER_TRUNCATED_SEQUENCE_OF_INT8));
  SECItem value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, SEQUENCE, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLength_SECItem_InvalidWrongTag)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_NOT_EMPTY, sizeof DER_SEQUENCE_NOT_EMPTY));
  SECItem value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, INTEGER, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetTLV_SECItem_ValidEmpty)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_EMPTY, sizeof DER_SEQUENCE_EMPTY));
  SECItem tlv = { siBuffer, nullptr, 5 };
  ASSERT_EQ(Success, ExpectTagAndGetTLV(input, SEQUENCE, tlv));
  ASSERT_EQ(sizeof DER_SEQUENCE_EMPTY, tlv.len);
  ASSERT_TRUE(tlv.data);
  ASSERT_FALSE(memcmp(tlv.data, DER_SEQUENCE_EMPTY,
                      sizeof DER_SEQUENCE_EMPTY));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, ExpectTagAndGetTLV_SECItem_ValidNotEmpty)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_NOT_EMPTY, sizeof DER_SEQUENCE_NOT_EMPTY));
  SECItem tlv;
  ASSERT_EQ(Success, ExpectTagAndGetTLV(input, SEQUENCE, tlv));
  ASSERT_EQ(sizeof(DER_SEQUENCE_NOT_EMPTY), tlv.len);
  ASSERT_TRUE(tlv.data);
  ASSERT_FALSE(memcmp(tlv.data, DER_SEQUENCE_NOT_EMPTY,
                      sizeof(DER_SEQUENCE_NOT_EMPTY)));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests,
       ExpectTagAndGetTLV_SECItem_InvalidNotEmptyValueTruncated)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_NOT_EMPTY_VALUE_TRUNCATED,
                       sizeof DER_SEQUENCE_NOT_EMPTY_VALUE_TRUNCATED));
  SECItem tlv;
  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndGetTLV(input, SEQUENCE, tlv));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetTLV_SECItem_InvalidWrongLength)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_TRUNCATED_SEQUENCE_OF_INT8,
                                sizeof DER_TRUNCATED_SEQUENCE_OF_INT8));
  SECItem tlv;
  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndGetTLV(input, SEQUENCE, tlv));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetTLV_SECItem_InvalidWrongTag)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_NOT_EMPTY, sizeof DER_SEQUENCE_NOT_EMPTY));
  SECItem tlv;
  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndGetTLV(input, INTEGER, tlv));
}

TEST_F(pkixder_input_tests, ExpectTagAndSkipLength)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_INT16, sizeof DER_INT16));
  ASSERT_EQ(Success, ExpectTagAndSkipLength(input, INTEGER));
}

TEST_F(pkixder_input_tests, ExpectTagAndSkipLengthWithWrongTag)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_INT16, sizeof DER_INT16));

  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndSkipLength(input, OCTET_STRING));
}

TEST_F(pkixder_input_tests, EndAtEnd)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_INT16, sizeof DER_INT16));
  ASSERT_EQ(Success, input.Skip(4));
  ASSERT_EQ(Success, End(input));
}

TEST_F(pkixder_input_tests, EndBeforeEnd)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_INT16, sizeof DER_INT16));
  ASSERT_EQ(Success, input.Skip(2));
  ASSERT_EQ(Result::ERROR_BAD_DER, End(input));
}

TEST_F(pkixder_input_tests, EndAtBeginning)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_INT16, sizeof DER_INT16));
  ASSERT_EQ(Result::ERROR_BAD_DER, End(input));
}

// TODO: Need tests for Nested too?

Result NestedOfHelper(Input& input, std::vector<uint8_t>& readValues)
{
  uint8_t value = 0;
  Result rv = input.Read(value);
  EXPECT_EQ(Success, rv);
  if (rv != Success) {
    return rv;
  }
  readValues.push_back(value);
  return Success;
}

TEST_F(pkixder_input_tests, NestedOf)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));

  std::vector<uint8_t> readValues;
  ASSERT_EQ(Success,
    NestedOf(input, SEQUENCE, INTEGER, EmptyAllowed::No,
             mozilla::pkix::bind(NestedOfHelper, mozilla::pkix::_1,
                                 mozilla::pkix::ref(readValues))));
  ASSERT_EQ((size_t) 3, readValues.size());
  ASSERT_EQ(0x01, readValues[0]);
  ASSERT_EQ(0x02, readValues[1]);
  ASSERT_EQ(0x03, readValues[2]);
  ASSERT_EQ(Success, End(input));
}

TEST_F(pkixder_input_tests, NestedOfWithTruncatedData)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_TRUNCATED_SEQUENCE_OF_INT8,
                                sizeof DER_TRUNCATED_SEQUENCE_OF_INT8));

  std::vector<uint8_t> readValues;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            NestedOf(input, SEQUENCE, INTEGER, EmptyAllowed::No,
                     mozilla::pkix::bind(NestedOfHelper, mozilla::pkix::_1,
                                         mozilla::pkix::ref(readValues))));
  ASSERT_EQ((size_t) 0, readValues.size());
}

TEST_F(pkixder_input_tests, MatchRestAtEnd)
{
  Input input;
  static const uint8_t der[1] = { };
  ASSERT_EQ(Success, input.Init(der, 0));
  ASSERT_TRUE(input.AtEnd());
  static const uint8_t toMatch[] = { 1 };
  ASSERT_FALSE(input.MatchRest(toMatch));
}

TEST_F(pkixder_input_tests, MatchRest1Match)
{
  Input input;
  static const uint8_t der[] = { 1 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));
  ASSERT_FALSE(input.AtEnd());
  ASSERT_TRUE(input.MatchRest(der));
}

TEST_F(pkixder_input_tests, MatchRest1Mismatch)
{
  Input input;
  static const uint8_t der[] = { 1 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));
  static const uint8_t toMatch[] = { 2 };
  ASSERT_FALSE(input.MatchRest(toMatch));
  ASSERT_FALSE(input.AtEnd());
}

TEST_F(pkixder_input_tests, MatchRest2WithTrailingByte)
{
  Input input;
  static const uint8_t der[] = { 1, 2, 3 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));
  static const uint8_t toMatch[] = { 1, 2 };
  ASSERT_FALSE(input.MatchRest(toMatch));
}

TEST_F(pkixder_input_tests, MatchRest2Mismatch)
{
  Input input;
  static const uint8_t der[] = { 1, 2, 3 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));
  static const uint8_t toMatchMismatch[] = { 1, 3 };
  ASSERT_FALSE(input.MatchRest(toMatchMismatch));
  ASSERT_TRUE(input.MatchRest(der));
}

} // unnamed namespace
