/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_DeserializedNode__
#define mozilla_devtools_DeserializedNode__

#include "mozilla/devtools/CoreDump.pb.h"
#include "mozilla/MaybeOneOf.h"
#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

// `Deserialized{Node,Edge}` translate protobuf messages from our core dump
// format into structures we can rely upon for implementing `JS::ubi::Node`
// specializations on top of. All of the properties of the protobuf messages are
// optional for future compatibility, and this is the layer where we validate
// that the properties that do actually exist in any given message fulfill our
// semantic requirements.
//
// Both `DeserializedNode` and `DeserializedEdge` are always owned by a
// `HeapSnapshot` instance, and their lifetimes must not extend after that of
// their owning `HeapSnapshot`.

namespace mozilla {
namespace devtools {

class HeapSnapshot;

using NodeId = uint64_t;

// A `DeserializedEdge` represents an edge in the heap graph pointing to the
// node with id equal to `DeserializedEdge::referent` that we deserialized from
// a core dump.
struct DeserializedEdge {
  NodeId         referent;
  // A borrowed reference to a string owned by this node's owning HeapSnapshot.
  const char16_t *name;

  explicit DeserializedEdge();
  DeserializedEdge(DeserializedEdge &&rhs);
  DeserializedEdge &operator=(DeserializedEdge &&rhs);

  // Initialize this `DeserializedEdge` from the given `protobuf::Edge` message.
  bool init(const protobuf::Edge &edge, HeapSnapshot &owner);

private:
  DeserializedEdge(const DeserializedEdge &) = delete;
  DeserializedEdge& operator=(const DeserializedEdge &) = delete;
};

// A `DeserializedNode` is a node in the heap graph that we deserialized from a
// core dump.
struct DeserializedNode {
  using EdgeVector = Vector<DeserializedEdge>;
  using UniqueStringPtr = UniquePtr<char16_t[]>;

  NodeId         id;
  // A borrowed reference to a string owned by this node's owning HeapSnapshot.
  const char16_t *typeName;
  uint64_t       size;
  EdgeVector     edges;
  // A weak pointer to this node's owning `HeapSnapshot`. Safe without
  // AddRef'ing because this node's lifetime is equal to that of its owner.
  HeapSnapshot   *owner;

  // Create a new `DeserializedNode` from the given `protobuf::Node` message.
  static UniquePtr<DeserializedNode> Create(const protobuf::Node &node,
                                            HeapSnapshot &owner);

  DeserializedNode(NodeId id,
                   const char16_t *typeName,
                   uint64_t size,
                   EdgeVector &&edges,
                   HeapSnapshot &owner);
  virtual ~DeserializedNode() { }

  // Get a borrowed reference to the given edge's referent. This method is
  // virtual to provide a hook for gmock and gtest.
  virtual DeserializedNode &getEdgeReferent(const DeserializedEdge &edge);

private:
  DeserializedNode(const DeserializedNode &) = delete;
  DeserializedNode &operator=(const DeserializedNode &) = delete;
};

} // namespace devtools
} // namespace mozilla

#endif // mozilla_devtools_DeserializedNode__
