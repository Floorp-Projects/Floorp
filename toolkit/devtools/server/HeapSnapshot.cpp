/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeapSnapshot.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "mozilla/devtools/DeserializedNode.h"
#include "mozilla/dom/HeapSnapshotBinding.h"

#include "CoreDump.pb.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCRTGlue.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace devtools {

using ::google::protobuf::io::ArrayInputStream;
using ::google::protobuf::io::CodedInputStream;
using ::google::protobuf::io::GzipInputStream;
using ::google::protobuf::io::ZeroCopyInputStream;

NS_IMPL_CYCLE_COLLECTION_CLASS(HeapSnapshot)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(HeapSnapshot)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(HeapSnapshot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(HeapSnapshot)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(HeapSnapshot)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HeapSnapshot)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HeapSnapshot)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* virtual */ JSObject*
HeapSnapshot::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::HeapSnapshotBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<HeapSnapshot>
HeapSnapshot::Create(JSContext* cx,
                     dom::GlobalObject& global,
                     const uint8_t* buffer,
                     uint32_t size,
                     ErrorResult& rv)
{
  nsRefPtr<HeapSnapshot> snapshot = new HeapSnapshot(cx, global.GetAsSupports());
  if (!snapshot->init(buffer, size)) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  return snapshot.forget();
}

template<typename MessageType>
static bool
parseMessage(ZeroCopyInputStream& stream, MessageType& message)
{
  // We need to create a new `CodedInputStream` for each message so that the
  // 64MB limit is applied per-message rather than to the whole stream.
  CodedInputStream codedStream(&stream);

  // Because protobuf messages aren't self-delimiting, we serialize each message
  // preceeded by its size in bytes. When deserializing, we read this size and
  // then limit reading from the stream to the given byte size. If we didn't,
  // then the first message would consume the entire stream.

  uint32_t size = 0;
  if (NS_WARN_IF(!codedStream.ReadVarint32(&size)))
    return false;

  auto limit = codedStream.PushLimit(size);
  if (NS_WARN_IF(!message.ParseFromCodedStream(&codedStream)) ||
      NS_WARN_IF(!codedStream.ConsumedEntireMessage()))
  {
    return false;
  }

  codedStream.PopLimit(limit);
  return true;
}

bool
HeapSnapshot::saveNode(const protobuf::Node& node)
{
  UniquePtr<DeserializedNode> dn(DeserializedNode::Create(node, *this));
  if (!dn)
    return false;
  return nodes.put(dn->id, Move(dn));
}

static inline bool
StreamHasData(GzipInputStream& stream)
{
  // Test for the end of the stream. The protobuf library gives no way to tell
  // the difference between an underlying read error and the stream being
  // done. All we can do is attempt to read data and extrapolate guestimations
  // from the result of that operation.

  const void* buf;
  int size;
  bool more = stream.Next(&buf, &size);
  if (!more)
    // Could not read any more data. We are optimistic and assume the stream is
    // just exhausted and there is not an underlying IO error, since this
    // function is only called at message boundaries.
    return false;

  // There is more data still available in the stream. Return the data we read
  // to the stream and let the parser get at it.
  stream.BackUp(size);
  return true;
}

bool
HeapSnapshot::init(const uint8_t* buffer, uint32_t size)
{
  if (!nodes.init() || !strings.init())
    return false;

  ArrayInputStream stream(buffer, size);
  GzipInputStream gzipStream(&stream);

  // First is the metadata.

  protobuf::Metadata metadata;
  if (!parseMessage(gzipStream, metadata))
    return false;
  if (metadata.has_timestamp())
    timestamp.emplace(metadata.timestamp());

  // Next is the root node.

  protobuf::Node root;
  if (!parseMessage(gzipStream, root))
    return false;

  // Although the id is optional in the protobuf format for future proofing, we
  // can't currently do anything without it.
  if (NS_WARN_IF(!root.has_id()))
    return false;
  rootId = root.id();

  if (NS_WARN_IF(!saveNode(root)))
    return false;

  // Finally, the rest of the nodes in the core dump.

  while (StreamHasData(gzipStream)) {
    protobuf::Node node;
    if (!parseMessage(gzipStream, node))
      return false;
    if (NS_WARN_IF(!saveNode(node)))
      return false;
  }

  return true;
}

const char16_t*
HeapSnapshot::borrowUniqueString(const char16_t* duplicateString, size_t length)
{
  MOZ_ASSERT(duplicateString);
  UniqueStringHashPolicy::Lookup lookup(duplicateString, length);
  auto ptr = strings.lookupForAdd(lookup);

  if (!ptr) {
    UniqueString owned(NS_strndup(duplicateString, length));
    if (!owned || !strings.add(ptr, Move(owned)))
      return nullptr;
  }

  MOZ_ASSERT(ptr->get() != duplicateString);
  return ptr->get();
}


} // namespace devtools
} // namespace mozilla
