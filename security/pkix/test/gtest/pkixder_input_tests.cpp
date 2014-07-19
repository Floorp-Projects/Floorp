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

TEST_F(pkixder_input_tests, InputBufferInit)
{
  InputBuffer buf;
  ASSERT_EQ(Success,
            buf.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));
}

TEST_F(pkixder_input_tests, InputBufferInitWithNullPointerOrZeroLength)
{
  InputBuffer buf;
  ASSERT_EQ(Result::ERROR_BAD_DER, buf.Init(nullptr, 0));

  ASSERT_EQ(Result::ERROR_BAD_DER, buf.Init(nullptr, 100));

  // Though it seems odd to initialize with zero-length and non-null ptr, this
  // is working as intended. The Input class was intended to protect against
  // buffer overflows, and there's no risk with the current behavior. See bug
  // 1000354.
  ASSERT_EQ(Success, buf.Init((const uint8_t*) "hello", 0));
  ASSERT_TRUE(buf.GetLength() == 0);
}

TEST_F(pkixder_input_tests, InputBufferInitWithLargeData)
{
  InputBuffer buf;
  // Data argument length does not matter, it is not touched, just
  // needs to be non-null
  ASSERT_EQ(Result::ERROR_BAD_DER, buf.Init((const uint8_t*) "", 0xffff+1));

  ASSERT_EQ(Success, buf.Init((const uint8_t*) "", 0xffff));
}

TEST_F(pkixder_input_tests, InputBufferInitMultipleTimes)
{
  InputBuffer buf;

  ASSERT_EQ(Success,
            buf.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));

  ASSERT_EQ(Result::FATAL_ERROR_INVALID_ARGS,
            buf.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));
}

TEST_F(pkixder_input_tests, PeekWithinBounds)
{
  const uint8_t der[] = { 0x11, 0x11 };
  InputBuffer buf(der);
  Input input(buf);
  ASSERT_TRUE(input.Peek(0x11));
  ASSERT_FALSE(input.Peek(0x22));
}

TEST_F(pkixder_input_tests, PeekPastBounds)
{
  const uint8_t der[] = { 0x11, 0x22 };
  InputBuffer buf;
  ASSERT_EQ(Success, buf.Init(der, 1));
  Input input(buf);

  uint8_t readByte;
  ASSERT_EQ(Success, input.Read(readByte));
  ASSERT_EQ(0x11, readByte);
  ASSERT_FALSE(input.Peek(0x22));
}

TEST_F(pkixder_input_tests, ReadByte)
{
  const uint8_t der[] = { 0x11, 0x22 };
  InputBuffer buf(der);
  Input input(buf);

  uint8_t readByte1;
  ASSERT_EQ(Success, input.Read(readByte1));
  ASSERT_EQ(0x11, readByte1);

  uint8_t readByte2;
  ASSERT_EQ(Success, input.Read(readByte2));
  ASSERT_EQ(0x22, readByte2);
}

TEST_F(pkixder_input_tests, ReadBytePastEnd)
{
  const uint8_t der[] = { 0x11, 0x22 };
  InputBuffer buf;
  ASSERT_EQ(Success, buf.Init(der, 1));
  Input input(buf);

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
  InputBuffer buf;
  ASSERT_EQ(Success, buf.Init(der, 0));
  Input input(buf);

  uint8_t b;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Read(b));
}

TEST_F(pkixder_input_tests, ReadWord)
{
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf(der);
  Input input(buf);

  uint16_t readWord1 = 0;
  ASSERT_EQ(Success, input.Read(readWord1));
  ASSERT_EQ(0x1122, readWord1);

  uint16_t readWord2 = 0;
  ASSERT_EQ(Success, input.Read(readWord2));
  ASSERT_EQ(0x3344, readWord2);
}

TEST_F(pkixder_input_tests, ReadWordPastEnd)
{
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf;
  ASSERT_EQ(Success, buf.Init(der, 2)); // Initialize with too-short length
  Input input(buf);

  uint16_t readWord1 = 0;
  ASSERT_EQ(Success, input.Read(readWord1));
  ASSERT_EQ(0x1122, readWord1);

  uint16_t readWord2 = 0;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Read(readWord2));
  ASSERT_NE(0x3344, readWord2);
}

TEST_F(pkixder_input_tests, ReadWordWithInsufficentData)
{
  const uint8_t der[] = { 0x11, 0x22 };
  InputBuffer buf;
  ASSERT_EQ(Success, buf.Init(der, 1));
  Input input(buf);

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
  InputBuffer buf;
  ASSERT_EQ(Success, buf.Init(der, 0));
  Input input(buf);
  uint16_t b;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Read(b));
}

TEST_F(pkixder_input_tests, InputSkip)
{
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf(der);
  Input input(buf);

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
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf(der);
  Input input(buf);
  ASSERT_EQ(Success, input.Skip(sizeof der));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, InputSkipPastEnd)
{
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf(der);
  Input input(buf);

  ASSERT_EQ(Result::ERROR_BAD_DER, input.Skip(sizeof der + 1));
}

TEST_F(pkixder_input_tests, InputSkipToNewInput)
{
  const uint8_t der[] = { 0x01, 0x02, 0x03, 0x04 };
  InputBuffer buf(der);
  Input input(buf);

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
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf(der);
  Input input(buf);

  Input skippedInput;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Skip(sizeof der * 2, skippedInput));
}

TEST_F(pkixder_input_tests, InputSkipToInputBuffer)
{
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf(der);
  Input input(buf);

  const uint8_t expectedItemData[] = { 0x11, 0x22, 0x33 };

  InputBuffer item;
  ASSERT_EQ(Success, input.Skip(sizeof expectedItemData, item));

  InputBuffer expected(expectedItemData);
  ASSERT_TRUE(InputBuffersAreEqual(expected, item));
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
  InputBuffer buf;
  ASSERT_EQ(Success, buf.Init(der, 0));
  Input input(buf);
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Skip(1));
}

TEST_F(pkixder_input_tests, SkipToInputBufferPastEnd)
{
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf(der);
  Input input(buf);

  InputBuffer skipped;
  ASSERT_EQ(Result::ERROR_BAD_DER, input.Skip(sizeof der + 1, skipped));
}

TEST_F(pkixder_input_tests, ExpectTagAndSkipValue)
{
  InputBuffer buf(DER_SEQUENCE_OF_INT8);
  Input input(buf);

  ASSERT_EQ(Success, ExpectTagAndSkipValue(input, SEQUENCE));
  ASSERT_EQ(Success, End(input));
}

TEST_F(pkixder_input_tests, ExpectTagAndSkipValueWithTruncatedData)
{
  InputBuffer buf(DER_TRUNCATED_SEQUENCE_OF_INT8);
  Input input(buf);

  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndSkipValue(input, SEQUENCE));
}

TEST_F(pkixder_input_tests, ExpectTagAndSkipValueWithOverrunData)
{
  InputBuffer buf(DER_OVERRUN_SEQUENCE_OF_INT8);
  Input input(buf);
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
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf(der);
  Input input(buf);
  ASSERT_FALSE(input.AtEnd());
}

TEST_F(pkixder_input_tests, AtEndAtEnd)
{
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf(der);
  Input input(buf);
  ASSERT_EQ(Success, input.Skip(sizeof der));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, MarkAndGetInputBuffer)
{
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf(der);
  Input input(buf);

  Input::Mark mark = input.GetMark();

  const uint8_t expectedItemData[] = { 0x11, 0x22, 0x33 };

  ASSERT_EQ(Success, input.Skip(sizeof expectedItemData));

  InputBuffer item;
  ASSERT_EQ(Success, input.GetInputBuffer(mark, item));
  InputBuffer expected(expectedItemData);
  ASSERT_TRUE(InputBuffersAreEqual(expected, item));
}

// Cannot run this test on debug builds because of the PR_NOT_REACHED
#ifndef DEBUG
TEST_F(pkixder_input_tests, MarkAndGetInputBufferDifferentInput)
{
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  InputBuffer buf(der);
  Input input(buf);

  Input another;
  Input::Mark mark = another.GetMark();

  ASSERT_EQ(Success, input.Skip(3));

  InputBuffer item;
  ASSERT_EQ(Result::FATAL_ERROR_INVALID_ARGS,
            input.GetInputBuffer(mark, item));
}
#endif

TEST_F(pkixder_input_tests, ExpectTagAndLength)
{
  InputBuffer buf(DER_SEQUENCE_OF_INT8);
  Input input(buf);

  ASSERT_EQ(Success, ExpectTagAndLength(input, SEQUENCE,
                                        sizeof DER_SEQUENCE_OF_INT8 - 2));
}

TEST_F(pkixder_input_tests, ExpectTagAndLengthWithWrongLength)
{
  InputBuffer buf(DER_INT16);
  Input input(buf);

  // Wrong length
  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndLength(input, INTEGER, 4));
}

TEST_F(pkixder_input_tests, ExpectTagAndLengthWithWrongTag)
{
  InputBuffer buf(DER_INT16);
  Input input(buf);

  // Wrong type
  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndLength(input, OCTET_STRING, 2));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLength)
{
  InputBuffer buf(DER_SEQUENCE_OF_INT8);
  Input input(buf);

  uint16_t length = 0;
  ASSERT_EQ(Success,
            der::internal::ExpectTagAndGetLength(input, SEQUENCE, length));
  ASSERT_EQ(sizeof DER_SEQUENCE_OF_INT8 - 2, length);
  ASSERT_EQ(Success, input.Skip(length));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLengthWithWrongTag)
{
  InputBuffer buf(DER_SEQUENCE_OF_INT8);
  Input input(buf);

  uint16_t length = 0;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            der::internal::ExpectTagAndGetLength(input, INTEGER, length));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLengthWithWrongLength)
{
  InputBuffer buf(DER_TRUNCATED_SEQUENCE_OF_INT8);
  Input input(buf);

  uint16_t length = 0;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            der::internal::ExpectTagAndGetLength(input, SEQUENCE, length));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_Input_ValidEmpty)
{
  InputBuffer buf(DER_SEQUENCE_EMPTY);
  Input input(buf);
  Input value;
  ASSERT_EQ(Success, ExpectTagAndGetValue(input, SEQUENCE, value));
  ASSERT_TRUE(value.AtEnd());
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_Input_ValidNotEmpty)
{
  InputBuffer buf(DER_SEQUENCE_NOT_EMPTY);
  Input input(buf);
  Input value;
  ASSERT_EQ(Success, ExpectTagAndGetValue(input, SEQUENCE, value));
  ASSERT_TRUE(value.MatchRest(DER_SEQUENCE_NOT_EMPTY_VALUE));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests,
       ExpectTagAndGetValue_Input_InvalidNotEmptyValueTruncated)
{
  InputBuffer buf(DER_SEQUENCE_NOT_EMPTY_VALUE_TRUNCATED);
  Input input(buf);
  Input value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, SEQUENCE, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_Input_InvalidWrongLength)
{
  InputBuffer buf(DER_TRUNCATED_SEQUENCE_OF_INT8);
  Input input(buf);
  Input value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, SEQUENCE, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLength_Input_InvalidWrongTag)
{
  InputBuffer buf(DER_SEQUENCE_NOT_EMPTY);
  Input input(buf);
  Input value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, INTEGER, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_InputBuffer_ValidEmpty)
{
  InputBuffer buf(DER_SEQUENCE_EMPTY);
  Input input(buf);
  InputBuffer value;
  ASSERT_EQ(Success, ExpectTagAndGetValue(input, SEQUENCE, value));
  ASSERT_EQ(0u, value.GetLength());
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_InputBuffer_ValidNotEmpty)
{
  InputBuffer buf(DER_SEQUENCE_NOT_EMPTY);
  Input input(buf);
  InputBuffer value;
  ASSERT_EQ(Success, ExpectTagAndGetValue(input, SEQUENCE, value));
  InputBuffer expected(DER_SEQUENCE_NOT_EMPTY_VALUE);
  ASSERT_TRUE(InputBuffersAreEqual(expected, value));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests,
       ExpectTagAndGetValue_InputBuffer_InvalidNotEmptyValueTruncated)
{
  InputBuffer buf(DER_SEQUENCE_NOT_EMPTY_VALUE_TRUNCATED);
  Input input(buf);
  InputBuffer value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, SEQUENCE, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetValue_InputBuffer_InvalidWrongLength)
{
  InputBuffer buf(DER_TRUNCATED_SEQUENCE_OF_INT8);
  Input input(buf);
  InputBuffer value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, SEQUENCE, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLength_InputBuffer_InvalidWrongTag)
{
  InputBuffer buf(DER_SEQUENCE_NOT_EMPTY);
  Input input(buf);
  InputBuffer value;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            ExpectTagAndGetValue(input, INTEGER, value));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetTLV_InputBuffer_ValidEmpty)
{
  InputBuffer buf(DER_SEQUENCE_EMPTY);
  Input input(buf);
  InputBuffer tlv;
  ASSERT_EQ(Success, ExpectTagAndGetTLV(input, SEQUENCE, tlv));
  InputBuffer expected(DER_SEQUENCE_EMPTY);
  ASSERT_TRUE(InputBuffersAreEqual(expected, tlv));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests, ExpectTagAndGetTLV_InputBuffer_ValidNotEmpty)
{
  InputBuffer buf(DER_SEQUENCE_NOT_EMPTY);
  Input input(buf);
  InputBuffer tlv;
  ASSERT_EQ(Success, ExpectTagAndGetTLV(input, SEQUENCE, tlv));
  InputBuffer expected(DER_SEQUENCE_NOT_EMPTY);
  ASSERT_TRUE(InputBuffersAreEqual(expected, tlv));
  ASSERT_TRUE(input.AtEnd());
}

TEST_F(pkixder_input_tests,
       ExpectTagAndGetTLV_InputBuffer_InvalidNotEmptyValueTruncated)
{
  InputBuffer buf(DER_SEQUENCE_NOT_EMPTY_VALUE_TRUNCATED);
  Input input(buf);
  InputBuffer tlv;
  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndGetTLV(input, SEQUENCE, tlv));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetTLV_InputBuffer_InvalidWrongLength)
{
  InputBuffer buf(DER_TRUNCATED_SEQUENCE_OF_INT8);
  Input input(buf);
  InputBuffer tlv;
  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndGetTLV(input, SEQUENCE, tlv));
}

TEST_F(pkixder_input_tests, ExpectTagAndGetTLV_InputBuffer_InvalidWrongTag)
{
  InputBuffer buf(DER_SEQUENCE_NOT_EMPTY);
  Input input(buf);
  InputBuffer tlv;
  ASSERT_EQ(Result::ERROR_BAD_DER, ExpectTagAndGetTLV(input, INTEGER, tlv));
}

TEST_F(pkixder_input_tests, EndAtEnd)
{
  InputBuffer buf(DER_INT16);
  Input input(buf);
  ASSERT_EQ(Success, input.Skip(4));
  ASSERT_EQ(Success, End(input));
}

TEST_F(pkixder_input_tests, EndBeforeEnd)
{
  InputBuffer buf(DER_INT16);
  Input input(buf);
  ASSERT_EQ(Success, input.Skip(2));
  ASSERT_EQ(Result::ERROR_BAD_DER, End(input));
}

TEST_F(pkixder_input_tests, EndAtBeginning)
{
  InputBuffer buf(DER_INT16);
  Input input(buf);
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
  InputBuffer buf(DER_SEQUENCE_OF_INT8);
  Input input(buf);

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
  InputBuffer buf(DER_TRUNCATED_SEQUENCE_OF_INT8);
  Input input(buf);

  std::vector<uint8_t> readValues;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            NestedOf(input, SEQUENCE, INTEGER, EmptyAllowed::No,
                     mozilla::pkix::bind(NestedOfHelper, mozilla::pkix::_1,
                                         mozilla::pkix::ref(readValues))));
  ASSERT_EQ((size_t) 0, readValues.size());
}

TEST_F(pkixder_input_tests, MatchRestAtEnd)
{
  static const uint8_t der[1] = { };
  InputBuffer buf;
  ASSERT_EQ(Success, buf.Init(der, 0));
  Input input(buf);
  ASSERT_TRUE(input.AtEnd());
  static const uint8_t toMatch[] = { 1 };
  ASSERT_FALSE(input.MatchRest(toMatch));
}

TEST_F(pkixder_input_tests, MatchRest1Match)
{
  static const uint8_t der[] = { 1 };
  InputBuffer buf(der);
  Input input(buf);
  ASSERT_FALSE(input.AtEnd());
  ASSERT_TRUE(input.MatchRest(der));
}

TEST_F(pkixder_input_tests, MatchRest1Mismatch)
{
  static const uint8_t der[] = { 1 };
  InputBuffer buf(der);
  Input input(buf);
  static const uint8_t toMatch[] = { 2 };
  ASSERT_FALSE(input.MatchRest(toMatch));
  ASSERT_FALSE(input.AtEnd());
}

TEST_F(pkixder_input_tests, MatchRest2WithTrailingByte)
{
  static const uint8_t der[] = { 1, 2, 3 };
  InputBuffer buf(der);
  Input input(buf);
  static const uint8_t toMatch[] = { 1, 2 };
  ASSERT_FALSE(input.MatchRest(toMatch));
}

TEST_F(pkixder_input_tests, MatchRest2Mismatch)
{
  static const uint8_t der[] = { 1, 2, 3 };
  InputBuffer buf(der);
  Input input(buf);
  static const uint8_t toMatchMismatch[] = { 1, 3 };
  ASSERT_FALSE(input.MatchRest(toMatchMismatch));
  ASSERT_TRUE(input.MatchRest(der));
}

} // unnamed namespace
