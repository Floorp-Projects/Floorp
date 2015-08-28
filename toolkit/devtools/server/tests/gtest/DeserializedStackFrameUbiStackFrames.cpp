/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the `JS::ubi::StackFrame`s we create from
// `mozilla::devtools::DeserializedStackFrame` instances look and behave as we would
// like.

#include "DevTools.h"
#include "js/TypeDecls.h"
#include "mozilla/devtools/DeserializedNode.h"

using testing::Field;
using testing::ReturnRef;

// A mock DeserializedStackFrame for testing.
struct MockDeserializedStackFrame : public DeserializedStackFrame
{
  MockDeserializedStackFrame() : DeserializedStackFrame() { }
};

DEF_TEST(DeserializedStackFrameUbiStackFrames, {
    StackFrameId id = uint64_t(1) << 42;
    uint32_t line = 1337;
    uint32_t column = 9; // 3 space tabs!?
    const char16_t* source = MOZ_UTF16("my-javascript-file.js");
    const char16_t* functionDisplayName = MOZ_UTF16("myFunctionName");

    MockDeserializedStackFrame mocked;
    mocked.id = id;
    mocked.line = line;
    mocked.column = column;
    mocked.source = source;
    mocked.functionDisplayName = functionDisplayName;

    DeserializedStackFrame& deserialized = mocked;
    JS::ubi::StackFrame ubiFrame(&deserialized);

    // Test the JS::ubi::StackFrame accessors.

    EXPECT_EQ(id, ubiFrame.identifier());
    EXPECT_EQ(JS::ubi::StackFrame(), ubiFrame.parent());
    EXPECT_EQ(line, ubiFrame.line());
    EXPECT_EQ(column, ubiFrame.column());
    EXPECT_EQ(JS::ubi::AtomOrTwoByteChars(source), ubiFrame.source());
    EXPECT_EQ(JS::ubi::AtomOrTwoByteChars(functionDisplayName),
              ubiFrame.functionDisplayName());
    EXPECT_FALSE(ubiFrame.isSelfHosted());
    EXPECT_FALSE(ubiFrame.isSystem());

    JS::RootedObject savedFrame(cx);
    EXPECT_TRUE(ubiFrame.constructSavedFrameStack(cx, &savedFrame));

    uint32_t frameLine;
    ASSERT_EQ(JS::SavedFrameResult::Ok, JS::GetSavedFrameLine(cx, savedFrame, &frameLine));
    EXPECT_EQ(line, frameLine);

    uint32_t frameColumn;
    ASSERT_EQ(JS::SavedFrameResult::Ok, JS::GetSavedFrameColumn(cx, savedFrame, &frameColumn));
    EXPECT_EQ(column, frameColumn);

    JS::RootedObject parent(cx);
    ASSERT_EQ(JS::SavedFrameResult::Ok, JS::GetSavedFrameParent(cx, savedFrame, &parent));
    EXPECT_EQ(nullptr, parent);

    ASSERT_EQ(NS_strlen(source), 21U);
    char16_t sourceBuf[21] = {};

    // Test when the length is shorter than the string length.
    auto written = ubiFrame.source(RangedPtr<char16_t>(sourceBuf), 3);
    EXPECT_EQ(written, 3U);
    for (size_t i = 0; i < 3; i++) {
      EXPECT_EQ(sourceBuf[i], source[i]);
    }

    written = ubiFrame.source(RangedPtr<char16_t>(sourceBuf), 21);
    EXPECT_EQ(written, 21U);
    for (size_t i = 0; i < 21; i++) {
      EXPECT_EQ(sourceBuf[i], source[i]);
    }

    ASSERT_EQ(NS_strlen(functionDisplayName), 14U);
    char16_t nameBuf[14] = {};

    written = ubiFrame.functionDisplayName(RangedPtr<char16_t>(nameBuf), 14);
    EXPECT_EQ(written, 14U);
    for (size_t i = 0; i < 14; i++) {
      EXPECT_EQ(nameBuf[i], functionDisplayName[i]);
    }
});
