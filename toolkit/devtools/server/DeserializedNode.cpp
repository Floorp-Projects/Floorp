/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/devtools/DeserializedNode.h"
#include "mozilla/devtools/HeapSnapshot.h"
#include "nsCRTGlue.h"

namespace mozilla {
namespace devtools {

DeserializedEdge::DeserializedEdge()
  : referent(0)
  , name(nullptr)
{ }

DeserializedEdge::DeserializedEdge(DeserializedEdge&& rhs)
{
  referent = rhs.referent;
  name = rhs.name;
}

DeserializedEdge& DeserializedEdge::operator=(DeserializedEdge&& rhs)
{
  MOZ_ASSERT(&rhs != this);
  this->~DeserializedEdge();
  new(this) DeserializedEdge(Move(rhs));
  return *this;
}

bool
DeserializedEdge::init(const protobuf::Edge& edge, HeapSnapshot& owner)
{
  // Although the referent property is optional in the protobuf format for
  // future compatibility, we can't semantically have an edge to nowhere and
  // require a referent here.
  if (!edge.has_referent())
    return false;
  referent = edge.referent();

  if (edge.has_name()) {
    const char16_t* duplicateEdgeName = reinterpret_cast<const char16_t*>(edge.name().c_str());
    name = owner.borrowUniqueString(duplicateEdgeName, edge.name().length() / sizeof(char16_t));
    if (!name)
      return false;
  }

  return true;
}

DeserializedNode::DeserializedNode(DeserializedNode&& rhs)
  : id(rhs.id)
  , typeName(rhs.typeName)
  , size(rhs.size)
  , edges(Move(rhs.edges))
  , allocationStack(rhs.allocationStack)
  , jsObjectClassName(Move(rhs.jsObjectClassName))
  , owner(rhs.owner)
{ }

DeserializedNode& DeserializedNode::operator=(DeserializedNode&& rhs)
{
  MOZ_ASSERT(&rhs != this);
  this->~DeserializedNode();
  new(this) DeserializedNode(Move(rhs));
  return *this;
}

DeserializedNode::DeserializedNode(NodeId id, const char16_t* typeName, uint64_t size)
  : id(id)
  , typeName(typeName)
  , size(size)
  , edges()
  , owner(nullptr)
{ }

JS::ubi::Node
DeserializedNode::getEdgeReferent(const DeserializedEdge& edge)
{
  auto ptr = owner->nodes.lookup(edge.referent);
  MOZ_ASSERT(ptr);

  // `HashSets` only provide const access to their values, because mutating a
  // value might change its hash, rendering it unfindable in the set.
  // Unfortunately, the `ubi::Node` constructor requires a non-const pointer to
  // its referent.  However, the only aspect of a `DeserializedNode` we hash on
  // is its id, which can't be changed via `ubi::Node`, so this cast can't cause
  // the trouble `HashSet` is concerned a non-const reference would cause.
  return JS::ubi::Node(const_cast<DeserializedNode*>(&*ptr));
}

JS::ubi::StackFrame
DeserializedStackFrame::getParentStackFrame() const
{
  MOZ_ASSERT(parent.isSome());
  auto ptr = owner->frames.lookup(parent.ref());
  MOZ_ASSERT(ptr);
  // See above comment in DeserializedNode::getEdgeReferent about why this
  // const_cast is needed and safe.
  return JS::ubi::StackFrame(const_cast<DeserializedStackFrame*>(&*ptr));
}

} // namespace devtools
} // namespace mozilla

namespace JS {
namespace ubi {

using mozilla::devtools::DeserializedEdge;

const char16_t Concrete<DeserializedNode>::concreteTypeName[] =
  MOZ_UTF16("mozilla::devtools::DeserializedNode");

const char16_t*
Concrete<DeserializedNode>::typeName() const
{
  return get().typeName;
}

size_t
Concrete<DeserializedNode>::size(mozilla::MallocSizeOf mallocSizeof) const
{
  return get().size;
}

class DeserializedEdgeRange : public EdgeRange
{
  SimpleEdgeVector edges;
  size_t           i;

  void settle() {
    front_ = i < edges.length() ? &edges[i] : nullptr;
  }

public:
  explicit DeserializedEdgeRange(JSContext* cx)
    : edges(cx)
    , i(0)
  {
    settle();
  }

  bool init(DeserializedNode& node)
  {
    if (!edges.reserve(node.edges.length()))
      return false;

    for (DeserializedEdge* edgep = node.edges.begin();
         edgep != node.edges.end();
         edgep++)
    {
      char16_t* name = nullptr;
      if (edgep->name) {
        name = NS_strdup(edgep->name);
        if (!name)
          return false;
      }

      auto referent = node.getEdgeReferent(*edgep);
      edges.infallibleAppend(mozilla::Move(SimpleEdge(name, referent)));
    }

    settle();
    return true;
  }

  void popFront() override
  {
    i++;
    settle();
  }
};

StackFrame
Concrete<DeserializedNode>::allocationStack() const
{
  MOZ_ASSERT(hasAllocationStack());
  auto id = get().allocationStack.ref();
  auto ptr = get().owner->frames.lookup(id);
  MOZ_ASSERT(ptr);
  // See above comment in DeserializedNode::getEdgeReferent about why this
  // const_cast is needed and safe.
  return JS::ubi::StackFrame(const_cast<DeserializedStackFrame*>(&*ptr));
}


UniquePtr<EdgeRange>
Concrete<DeserializedNode>::edges(JSContext* cx, bool) const
{
  UniquePtr<DeserializedEdgeRange, JS::DeletePolicy<DeserializedEdgeRange>> range(
    js_new<DeserializedEdgeRange>(cx));

  if (!range || !range->init(get()))
    return nullptr;

  return UniquePtr<EdgeRange>(range.release());
}

StackFrame
ConcreteStackFrame<DeserializedStackFrame>::parent() const
{
  return get().parent.isNothing() ? StackFrame() : get().getParentStackFrame();
}

bool
ConcreteStackFrame<DeserializedStackFrame>::constructSavedFrameStack(
  JSContext* cx,
  MutableHandleObject outSavedFrameStack) const
{
  StackFrame f(&get());
  return ConstructSavedFrameStackSlow(cx, f, outSavedFrameStack);
}

} // namespace ubi
} // namespace JS
