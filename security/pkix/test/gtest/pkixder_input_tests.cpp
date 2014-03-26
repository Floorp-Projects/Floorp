/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Copyright 2013 Mozilla Foundation
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

using namespace mozilla::pkix::der;

namespace {

class pkixder_input_tests : public ::testing::Test
{
protected:
  virtual void SetUp()
  {
    PR_SetError(0, 0);
  }
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

TEST_F(pkixder_input_tests, FailWithError)
{
  ASSERT_EQ(Failure, Fail(SEC_ERROR_BAD_DER));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());

  ASSERT_EQ(Failure, Fail(SEC_ERROR_INVALID_ARGS));
  ASSERT_EQ(SEC_ERROR_INVALID_ARGS, PR_GetError());
}

TEST_F(pkixder_input_tests, InputInit)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));
}

TEST_F(pkixder_input_tests, InputInitWithNullPointerOrZeroLength)
{
  Input input;
  ASSERT_EQ(Failure, input.Init(nullptr, 0));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());

  ASSERT_EQ(Failure, input.Init(nullptr, 100));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());

  // Is this a bug?
  ASSERT_EQ(Success, input.Init((const uint8_t*) "hello", 0));
}

TEST_F(pkixder_input_tests, InputInitWithLargeData)
{
  Input input;
  // Data argument length does not matter, it is not touched, just
  // needs to be non-null
  ASSERT_EQ(Failure, input.Init((const uint8_t*) "", 0xffff+1));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());

  ASSERT_EQ(Success, input.Init((const uint8_t*) "", 0xffff));
}

TEST_F(pkixder_input_tests, InputInitMultipleTimes)
{
  Input input;

  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));

  ASSERT_EQ(Failure,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));
  ASSERT_EQ(SEC_ERROR_INVALID_ARGS, PR_GetError());
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
  ASSERT_EQ(Failure, input.Expect(expected, sizeof expected));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_input_tests, ExpectTooMuch)
{
  Input input;

  const uint8_t der[] = { 0x11, 0x22 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  const uint8_t expected[] = { 0x11, 0x22, 0x33 };
  ASSERT_EQ(Failure, input.Expect(expected, sizeof expected));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
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
  ASSERT_EQ(Failure, input.Read(readByte2));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
  ASSERT_NE(0x22, readByte2);
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
  ASSERT_EQ(Failure, input.Read(readWord2));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
  ASSERT_NE(0x3344, readWord2);
}

TEST_F(pkixder_input_tests, ReadWordWithInsufficentData)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22 };
  ASSERT_EQ(Success, input.Init(der, 1));

  uint16_t readWord1 = 0;
  ASSERT_EQ(Failure, input.Read(readWord1));
  ASSERT_NE(0x1122, readWord1);
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

  ASSERT_EQ(Failure, input.Skip(sizeof der + 1));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
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
  ASSERT_EQ(Failure, input.Skip(sizeof der * 2, skippedInput));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
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

TEST_F(pkixder_input_tests, SkipToSECItemPastEnd)
{
  Input input;
  const uint8_t der[] = { 0x11, 0x22, 0x33, 0x44 };
  ASSERT_EQ(Success, input.Init(der, sizeof der));

  SECItem skippedSECItem;
  ASSERT_EQ(Failure, input.Skip(sizeof der + 1, skippedSECItem));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_input_tests, Skip)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));

  ASSERT_EQ(Success, Skip(input, SEQUENCE));
  ASSERT_EQ(Success, End(input));
}

TEST_F(pkixder_input_tests, SkipWithTruncatedData)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_TRUNCATED_SEQUENCE_OF_INT8,
                                sizeof DER_TRUNCATED_SEQUENCE_OF_INT8));

  ASSERT_EQ(Failure, Skip(input, SEQUENCE));
}

TEST_F(pkixder_input_tests, SkipWithOverrunData)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_OVERRUN_SEQUENCE_OF_INT8,
                                sizeof DER_OVERRUN_SEQUENCE_OF_INT8));
  ASSERT_EQ(Success, Skip(input, SEQUENCE));
  ASSERT_EQ(Failure, End(input));
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

  ASSERT_TRUE(input.GetSECItem(siBuffer, mark, item));
  ASSERT_EQ(siBuffer, item.type);
  ASSERT_EQ(sizeof expectedItemData, item.len);
  ASSERT_TRUE(item.data);
  ASSERT_EQ(0, memcmp(item.data, expectedItemData, sizeof expectedItemData));
}

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
  ASSERT_EQ(Failure, ExpectTagAndLength(input, INTEGER, 4));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_input_tests, ExpectTagAndLengthWithWrongTag)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_INT16, sizeof DER_INT16));

  // Wrong type
  ASSERT_EQ(Failure, ExpectTagAndLength(input, OCTET_STRING, 2));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLength)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_SEQUENCE_OF_INT8, sizeof DER_SEQUENCE_OF_INT8));

  uint16_t length = 0;
  ASSERT_EQ(Success, ExpectTagAndGetLength(input, SEQUENCE, length));
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
  ASSERT_EQ(Failure, ExpectTagAndGetLength(input, INTEGER, length));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_input_tests, ExpectTagAndGetLengthWithWrongLength)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_TRUNCATED_SEQUENCE_OF_INT8,
                                sizeof DER_TRUNCATED_SEQUENCE_OF_INT8));

  uint16_t length = 0;
  ASSERT_EQ(Failure, ExpectTagAndGetLength(input, SEQUENCE, length));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_input_tests, ExpectTagAndIgnoreLength)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_INT16, sizeof DER_INT16));
  ASSERT_EQ(Success, ExpectTagAndIgnoreLength(input, INTEGER));
}

TEST_F(pkixder_input_tests, ExpectTagAndIgnoreLengthWithWrongTag)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_INT16, sizeof DER_INT16));

  ASSERT_EQ(Failure, ExpectTagAndIgnoreLength(input, OCTET_STRING));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
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
  ASSERT_EQ(Failure, End(input));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_input_tests, EndAtBeginning)
{
  Input input;
  ASSERT_EQ(Success, input.Init(DER_INT16, sizeof DER_INT16));
  ASSERT_EQ(Failure, End(input));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

// TODO: Need tests for Nested too?

Result NestedOfHelper(Input& input, std::vector<uint8_t>& readValues)
{
  uint8_t value = 0;
  if (input.Read(value) != Success) {
    return Failure;
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
    NestedOf(input, SEQUENCE, INTEGER, MustNotBeEmpty,
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
  ASSERT_EQ(Failure,
    NestedOf(input, SEQUENCE, INTEGER, MustNotBeEmpty,
             mozilla::pkix::bind(NestedOfHelper, mozilla::pkix::_1,
                                 mozilla::pkix::ref(readValues))));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
  ASSERT_EQ((size_t) 0, readValues.size());
}
} // unnamed namespace
