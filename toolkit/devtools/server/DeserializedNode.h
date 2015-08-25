/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_DeserializedNode__
#define mozilla_devtools_DeserializedNode__

#include "js/UbiNode.h"
#include "mozilla/devtools/CoreDump.pb.h"
#include "mozilla/Maybe.h"
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
using StackFrameId = uint64_t;

// A `DeserializedEdge` represents an edge in the heap graph pointing to the
// node with id equal to `DeserializedEdge::referent` that we deserialized from
// a core dump.
struct DeserializedEdge {
  NodeId         referent;
  // A borrowed reference to a string owned by this node's owning HeapSnapshot.
  const char16_t* name;

  explicit DeserializedEdge();
  DeserializedEdge(DeserializedEdge&& rhs);
  DeserializedEdge& operator=(DeserializedEdge&& rhs);

  // Initialize this `DeserializedEdge` from the given `protobuf::Edge` message.
  bool init(const protobuf::Edge& edge, HeapSnapshot& owner);

private:
  DeserializedEdge(const DeserializedEdge&) = delete;
  DeserializedEdge& operator=(const DeserializedEdge&) = delete;
};

// A `DeserializedNode` is a node in the heap graph that we deserialized from a
// core dump.
struct DeserializedNode {
  using EdgeVector = Vector<DeserializedEdge>;
  using UniqueStringPtr = UniquePtr<char16_t[]>;

  NodeId              id;
  // A borrowed reference to a string owned by this node's owning HeapSnapshot.
  const char16_t*     typeName;
  uint64_t            size;
  EdgeVector          edges;
  Maybe<StackFrameId> allocationStack;
  UniquePtr<char[]>   jsObjectClassName;
  // A weak pointer to this node's owning `HeapSnapshot`. Safe without
  // AddRef'ing because this node's lifetime is equal to that of its owner.
  HeapSnapshot*       owner;

  DeserializedNode(NodeId id,
                   const char16_t* typeName,
                   uint64_t size,
                   EdgeVector&& edges,
                   Maybe<StackFrameId> allocationStack,
                   UniquePtr<char[]>&& className,
                   HeapSnapshot& owner)
    : id(id)
    , typeName(typeName)
    , size(size)
    , edges(Move(edges))
    , allocationStack(allocationStack)
    , jsObjectClassName(Move(className))
    , owner(&owner)
  { }
  virtual ~DeserializedNode() { }

  DeserializedNode(DeserializedNode&& rhs);
  DeserializedNode& operator=(DeserializedNode&& rhs);

  // Get a borrowed reference to the given edge's referent. This method is
  // virtual to provide a hook for gmock and gtest.
  virtual JS::ubi::Node getEdgeReferent(const DeserializedEdge& edge);

  struct HashPolicy;

protected:
  // This is only for use with `MockDeserializedNode` in testing.
  DeserializedNode(NodeId id, const char16_t* typeName, uint64_t size);

private:
  DeserializedNode(const DeserializedNode&) = delete;
  DeserializedNode& operator=(const DeserializedNode&) = delete;
};

static inline js::HashNumber
hashIdDerivedFromPtr(uint64_t id)
{
    // NodeIds and StackFrameIds are always 64 bits, but they are derived from
    // the original referents' addresses, which could have been either 32 or 64
    // bits long. As such, NodeId and StackFrameId have little entropy in their
    // bottom three bits, and may or may not have entropy in their upper 32
    // bits. This hash should manage both cases well.
    id >>= 3;
    return js::HashNumber((id >> 32) ^ id);
}

struct DeserializedNode::HashPolicy
{
  using Lookup = NodeId;

  static js::HashNumber hash(const Lookup& lookup) {
    return hashIdDerivedFromPtr(lookup);
  }

  static bool match(const DeserializedNode& existing, const Lookup& lookup) {
    return existing.id == lookup;
  }
};

// A `DeserializedStackFrame` is a stack frame referred to by a thing in the
// heap graph that we deserialized from a core dump.
struct DeserializedStackFrame {
  StackFrameId        id;
  Maybe<StackFrameId> parent;
  uint32_t            line;
  uint32_t            column;
  // Borrowed references to strings owned by this DeserializedStackFrame's
  // owning HeapSnapshot.
  const char16_t*     source;
  const char16_t*     functionDisplayName;
  bool                isSystem;
  bool                isSelfHosted;
  // A weak pointer to this frame's owning `HeapSnapshot`. Safe without
  // AddRef'ing because this frame's lifetime is equal to that of its owner.
  HeapSnapshot*       owner;

  explicit DeserializedStackFrame(StackFrameId id,
                                  const Maybe<StackFrameId>& parent,
                                  uint32_t line,
                                  uint32_t column,
                                  const char16_t* source,
                                  const char16_t* functionDisplayName,
                                  bool isSystem,
                                  bool isSelfHosted,
                                  HeapSnapshot& owner)
    : id(id)
    , parent(parent)
    , line(line)
    , column(column)
    , source(source)
    , functionDisplayName(functionDisplayName)
    , isSystem(isSystem)
    , isSelfHosted(isSelfHosted)
    , owner(&owner)
  {
    MOZ_ASSERT(source);
  }

  JS::ubi::StackFrame getParentStackFrame() const;

  struct HashPolicy;

protected:
  // This is exposed only for MockDeserializedStackFrame in the gtests.
  explicit DeserializedStackFrame()
    : id(0)
    , parent(Nothing())
    , line(0)
    , column(0)
    , source(nullptr)
    , functionDisplayName(nullptr)
    , isSystem(false)
    , isSelfHosted(false)
    , owner(nullptr)
  { };
};

struct DeserializedStackFrame::HashPolicy {
  using Lookup = StackFrameId;

  static js::HashNumber hash(const Lookup& lookup) {
    return hashIdDerivedFromPtr(lookup);
  }

  static bool match(const DeserializedStackFrame& existing, const Lookup& lookup) {
    return existing.id == lookup;
  }
};

} // namespace devtools
} // namespace mozilla

namespace JS {
namespace ubi {

using mozilla::devtools::DeserializedNode;
using mozilla::devtools::DeserializedStackFrame;
using mozilla::UniquePtr;

template<>
struct Concrete<DeserializedNode> : public Base
{
protected:
  explicit Concrete(DeserializedNode* ptr) : Base(ptr) { }
  DeserializedNode& get() const {
    return *static_cast<DeserializedNode*>(ptr);
  }

public:
  static const char16_t concreteTypeName[];

  static void construct(void* storage, DeserializedNode* ptr) {
    new (storage) Concrete(ptr);
  }

  Id identifier() const override { return get().id; }
  bool isLive() const override { return false; }
  const char16_t* typeName() const override;
  size_t size(mozilla::MallocSizeOf mallocSizeof) const override;
  const char* jsObjectClassName() const override { return get().jsObjectClassName.get(); }

  // We ignore the `bool wantNames` parameter because we can't control whether
  // the core dump was serialized with edge names or not.
  UniquePtr<EdgeRange> edges(JSContext* cx, bool) const override;
};

template<>
class ConcreteStackFrame<DeserializedStackFrame> : public BaseStackFrame
{
protected:
  explicit ConcreteStackFrame(DeserializedStackFrame* ptr)
    : BaseStackFrame(ptr)
  { }

  DeserializedStackFrame& get() const {
    return *static_cast<DeserializedStackFrame*>(ptr);
  }

public:
  static void construct(void* storage, DeserializedStackFrame* ptr) {
    new (storage) ConcreteStackFrame(ptr);
  }

  uintptr_t identifier() const override { return get().id; }
  uint32_t line() const override { return get().line; }
  uint32_t column() const override { return get().column; }
  bool isSystem() const override { return get().isSystem; }
  bool isSelfHosted() const override { return get().isSelfHosted; }
  void trace(JSTracer* trc) override { }
  AtomOrTwoByteChars source() const override {
    return AtomOrTwoByteChars(get().source);
  }
  AtomOrTwoByteChars functionDisplayName() const override {
    return AtomOrTwoByteChars(get().functionDisplayName);
  }

  StackFrame parent() const override;
  bool constructSavedFrameStack(JSContext* cx,
                                MutableHandleObject outSavedFrameStack)
    const override;
};

} // namespace ubi
} // namespace JS

#endif // mozilla_devtools_DeserializedNode__
