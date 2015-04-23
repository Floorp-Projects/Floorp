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

/* static */ UniquePtr<DeserializedNode>
DeserializedNode::Create(const protobuf::Node& node, HeapSnapshot& owner)
{
  if (!node.has_id())
    return nullptr;
  NodeId id = node.id();

  if (!node.has_typename_())
    return nullptr;

  const char16_t* duplicatedTypeName = reinterpret_cast<const char16_t*>(node.typename_().c_str());
  const char16_t* uniqueTypeName = owner.borrowUniqueString(duplicatedTypeName,
                                                            node.typename_().length() / sizeof(char16_t));
  if (!uniqueTypeName)
    return nullptr;

  auto edgesLength = node.edges_size();
  EdgeVector edges;
  if (!edges.reserve(edgesLength))
    return nullptr;
  for (decltype(edgesLength) i = 0; i < edgesLength; i++) {
    DeserializedEdge edge;
    if (!edge.init(node.edges(i), owner))
      return nullptr;
    edges.infallibleAppend(Move(edge));
  }

  if (!node.has_size())
    return nullptr;
  uint64_t size = node.size();

  return MakeUnique<DeserializedNode>(id,
                                      uniqueTypeName,
                                      size,
                                      Move(edges),
                                      owner);
}

DeserializedNode::DeserializedNode(NodeId id,
                                   const char16_t* typeName,
                                   uint64_t size,
                                   EdgeVector&& edges,
                                   HeapSnapshot& owner)
  : id(id)
  , typeName(typeName)
  , size(size)
  , edges(Move(edges))
  , owner(&owner)
{ }

DeserializedNode::DeserializedNode(NodeId id, const char16_t* typeName, uint64_t size)
  : id(id)
  , typeName(typeName)
  , size(size)
  , edges()
  , owner(nullptr)
{ }

DeserializedNode&
DeserializedNode::getEdgeReferent(const DeserializedEdge& edge)
{
  auto ptr = owner->nodes.lookup(edge.referent);
  MOZ_ASSERT(ptr);
  return *ptr->value();
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

      DeserializedNode& referent = node.getEdgeReferent(*edgep);
      edges.infallibleAppend(mozilla::Move(SimpleEdge(name, Node(&referent))));
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

UniquePtr<EdgeRange>
Concrete<DeserializedNode>::edges(JSContext* cx, bool) const
{
  UniquePtr<DeserializedEdgeRange, JS::DeletePolicy<DeserializedEdgeRange>> range(
    js_new<DeserializedEdgeRange>(cx));

  if (!range || !range->init(get()))
    return nullptr;

  return UniquePtr<EdgeRange>(range.release());
}

} // namespace JS
} // namespace ubi
