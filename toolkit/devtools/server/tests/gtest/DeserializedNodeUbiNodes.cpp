/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the `JS::ubi::Node`s we create from
// `mozilla::devtools::DeserializedNode` instances look and behave as we would
// like.

#include "DevTools.h"
#include "js/TypeDecls.h"
#include "mozilla/devtools/DeserializedNode.h"

using testing::Field;
using testing::ReturnRef;

// A mock DeserializedNode for testing.
struct MockDeserializedNode : public DeserializedNode
{
  MockDeserializedNode(NodeId id, const char16_t* typeName, uint64_t size)
    : DeserializedNode(id, typeName, size)
  { }

  bool addEdge(DeserializedEdge&& edge)
  {
    return edges.append(Move(edge));
  }

  MOCK_METHOD1(getEdgeReferent, JS::ubi::Node(const DeserializedEdge&));
};

size_t fakeMallocSizeOf(const void*) {
  EXPECT_TRUE(false);
  MOZ_ASSERT_UNREACHABLE("fakeMallocSizeOf should never be called because "
                         "DeserializedNodes report the deserialized size.");
  return 0;
}

DEF_TEST(DeserializedNodeUbiNodes, {
    const char16_t* typeName = MOZ_UTF16("TestTypeName");
    const char* className = "MyObjectClassName";

    NodeId id = uint64_t(1) << 33;
    uint64_t size = uint64_t(1) << 60;
    MockDeserializedNode mocked(id, typeName, size);
    mocked.jsObjectClassName = mozilla::UniquePtr<char[]>(strdup(className));
    ASSERT_TRUE(!!mocked.jsObjectClassName);
    mocked.coarseType = JS::ubi::CoarseType::Script;

    DeserializedNode& deserialized = mocked;
    JS::ubi::Node ubi(&deserialized);

    // Test the ubi::Node accessors.

    EXPECT_EQ(size, ubi.size(fakeMallocSizeOf));
    EXPECT_EQ(typeName, ubi.typeName());
    EXPECT_EQ(JS::ubi::CoarseType::Script, ubi.coarseType());
    EXPECT_EQ(id, ubi.identifier());
    EXPECT_FALSE(ubi.isLive());
    EXPECT_EQ(strcmp(ubi.jsObjectClassName(), className), 0);

    // Test the ubi::Node's edges.

    UniquePtr<DeserializedNode> referent1(new MockDeserializedNode(1,
                                                                   nullptr,
                                                                   10));
    DeserializedEdge edge1;
    edge1.referent = referent1->id;
    mocked.addEdge(Move(edge1));
    EXPECT_CALL(mocked,
                getEdgeReferent(Field(&DeserializedEdge::referent,
                                      referent1->id)))
      .Times(1)
      .WillOnce(Return(JS::ubi::Node(referent1.get())));

    UniquePtr<DeserializedNode> referent2(new MockDeserializedNode(2,
                                                                   nullptr,
                                                                   20));
    DeserializedEdge edge2;
    edge2.referent = referent2->id;
    mocked.addEdge(Move(edge2));
    EXPECT_CALL(mocked,
                getEdgeReferent(Field(&DeserializedEdge::referent,
                                      referent2->id)))
      .Times(1)
      .WillOnce(Return(JS::ubi::Node(referent2.get())));

    UniquePtr<DeserializedNode> referent3(new MockDeserializedNode(3,
                                                                   nullptr,
                                                                   30));
    DeserializedEdge edge3;
    edge3.referent = referent3->id;
    mocked.addEdge(Move(edge3));
    EXPECT_CALL(mocked,
                getEdgeReferent(Field(&DeserializedEdge::referent,
                                      referent3->id)))
      .Times(1)
      .WillOnce(Return(JS::ubi::Node(referent3.get())));

    ubi.edges(cx);
  });
