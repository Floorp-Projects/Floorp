/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeUtils.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>

#include "mozilla/devtools/HeapSnapshot.h"
#include "mozilla/devtools/ZeroCopyNSIOutputStream.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"

#include "nsCRTGlue.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "prerror.h"
#include "prio.h"
#include "prtypes.h"

#include "js/Debug.h"
#include "js/TypeDecls.h"
#include "js/UbiNodeTraverse.h"

namespace mozilla {
namespace devtools {

using namespace JS;
using namespace dom;


// If we are only taking a snapshot of the heap affected by the given set of
// globals, find the set of zones the globals are allocated within. Returns
// false on OOM failure.
static bool
PopulateZonesWithGlobals(ZoneSet& zones, AutoObjectVector& globals)
{
  if (!zones.init())
    return false;

  unsigned length = globals.length();
  for (unsigned i = 0; i < length; i++) {
    if (!zones.put(GetTenuredGCThingZone(globals[i])))
      return false;
  }

  return true;
}

// Add the given set of globals as explicit roots in the given roots
// list. Returns false on OOM failure.
static bool
AddGlobalsAsRoots(AutoObjectVector& globals, ubi::RootList& roots)
{
  unsigned length = globals.length();
  for (unsigned i = 0; i < length; i++) {
    if (!roots.addRoot(ubi::Node(globals[i].get()),
                       MOZ_UTF16("heap snapshot global")))
    {
      return false;
    }
  }
  return true;
}

// Choose roots and limits for a traversal, given `boundaries`. Set `roots` to
// the set of nodes within the boundaries that are referred to by nodes
// outside. If `boundaries` does not include all JS zones, initialize `zones` to
// the set of included zones; otherwise, leave `zones` uninitialized. (You can
// use zones.initialized() to check.)
//
// If `boundaries` is incoherent, or we encounter an error while trying to
// handle it, or we run out of memory, set `rv` appropriately and return
// `false`.
static bool
EstablishBoundaries(JSContext* cx,
                    ErrorResult& rv,
                    const HeapSnapshotBoundaries& boundaries,
                    ubi::RootList& roots,
                    ZoneSet& zones)
{
  MOZ_ASSERT(!roots.initialized());
  MOZ_ASSERT(!zones.initialized());

  bool foundBoundaryProperty = false;

  if (boundaries.mRuntime.WasPassed()) {
    foundBoundaryProperty = true;

    if (!boundaries.mRuntime.Value()) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }

    if (!roots.init()) {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return false;
    }
  }

  if (boundaries.mDebugger.WasPassed()) {
    if (foundBoundaryProperty) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }
    foundBoundaryProperty = true;

    JSObject* dbgObj = boundaries.mDebugger.Value();
    if (!dbgObj || !dbg::IsDebugger(*dbgObj)) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }

    AutoObjectVector globals(cx);
    if (!dbg::GetDebuggeeGlobals(cx, *dbgObj, globals) ||
        !PopulateZonesWithGlobals(zones, globals) ||
        !roots.init(zones) ||
        !AddGlobalsAsRoots(globals, roots))
    {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return false;
    }
  }

  if (boundaries.mGlobals.WasPassed()) {
    if (foundBoundaryProperty) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }
    foundBoundaryProperty = true;

    uint32_t length = boundaries.mGlobals.Value().Length();
    if (length == 0) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }

    AutoObjectVector globals(cx);
    for (uint32_t i = 0; i < length; i++) {
      JSObject* global = boundaries.mGlobals.Value().ElementAt(i);
      if (!JS_IsGlobalObject(global)) {
        rv.Throw(NS_ERROR_INVALID_ARG);
        return false;
      }
      if (!globals.append(global)) {
        rv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return false;
      }
    }

    if (!PopulateZonesWithGlobals(zones, globals) ||
        !roots.init(zones) ||
        !AddGlobalsAsRoots(globals, roots))
    {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return false;
    }
  }

  if (!foundBoundaryProperty) {
    rv.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  MOZ_ASSERT(roots.initialized());
  MOZ_ASSERT_IF(boundaries.mDebugger.WasPassed(), zones.initialized());
  MOZ_ASSERT_IF(boundaries.mGlobals.WasPassed(), zones.initialized());
  return true;
}


// A `CoreDumpWriter` that serializes nodes to protobufs and writes them to the
// given `ZeroCopyOutputStream`.
class MOZ_STACK_CLASS StreamWriter : public CoreDumpWriter
{
  JSContext* cx;
  bool      wantNames;

  ::google::protobuf::io::ZeroCopyOutputStream& stream;

  bool writeMessage(const ::google::protobuf::MessageLite& message) {
    // We have to create a new CodedOutputStream when writing each message so
    // that the 64MB size limit used by Coded{Output,Input}Stream to prevent
    // integer overflow is enforced per message rather than on the whole stream.
    ::google::protobuf::io::CodedOutputStream codedStream(&stream);
    codedStream.WriteVarint32(message.ByteSize());
    message.SerializeWithCachedSizes(&codedStream);
    return !codedStream.HadError();
  }

public:
  StreamWriter(JSContext* cx,
               ::google::protobuf::io::ZeroCopyOutputStream& stream,
               bool wantNames)
    : cx(cx)
    , wantNames(wantNames)
    , stream(stream)
  { }

  ~StreamWriter() override { }

  virtual bool writeMetadata(uint64_t timestamp) override {
    protobuf::Metadata metadata;
    metadata.set_timestamp(timestamp);
    return writeMessage(metadata);
  }

  virtual bool writeNode(const JS::ubi::Node& ubiNode,
                         EdgePolicy includeEdges) override {
    protobuf::Node protobufNode;
    protobufNode.set_id(ubiNode.identifier());

    const char16_t* typeName = ubiNode.typeName();
    size_t length = NS_strlen(typeName) * sizeof(char16_t);
    protobufNode.set_typename_(typeName, length);

    JSRuntime* rt = JS_GetRuntime(cx);
    mozilla::MallocSizeOf mallocSizeOf = dbg::GetDebuggerMallocSizeOf(rt);
    MOZ_ASSERT(mallocSizeOf);
    protobufNode.set_size(ubiNode.size(mallocSizeOf));

    if (includeEdges) {
      auto edges = ubiNode.edges(cx, wantNames);
      if (NS_WARN_IF(!edges))
        return false;

      for ( ; !edges->empty(); edges->popFront()) {
        const ubi::Edge& ubiEdge = edges->front();

        protobuf::Edge* protobufEdge = protobufNode.add_edges();
        if (NS_WARN_IF(!protobufEdge)) {
          return false;
        }

        protobufEdge->set_referent(ubiEdge.referent.identifier());

        if (wantNames && ubiEdge.name) {
          size_t length = NS_strlen(ubiEdge.name) * sizeof(char16_t);
          protobufEdge->set_name(ubiEdge.name, length);
        }
      }
    }

    return writeMessage(protobufNode);
  }
};

// A JS::ubi::BreadthFirst handler that serializes a snapshot of the heap into a
// core dump.
class MOZ_STACK_CLASS HeapSnapshotHandler {
  CoreDumpWriter& writer;
  JS::ZoneSet*    zones;

public:
  HeapSnapshotHandler(CoreDumpWriter& writer,
                      JS::ZoneSet* zones)
    : writer(writer),
      zones(zones)
  { }

  // JS::ubi::BreadthFirst handler interface.

  class NodeData { };
  typedef JS::ubi::BreadthFirst<HeapSnapshotHandler> Traversal;
  bool operator() (Traversal& traversal,
                   JS::ubi::Node origin,
                   const JS::ubi::Edge& edge,
                   NodeData*,
                   bool first)
  {
    // We're only interested in the first time we reach edge.referent, not in
    // every edge arriving at that node. "But, don't we want to serialize every
    // edge in the heap graph?" you ask. Don't worry! This edge is still
    // serialized into the core dump. Serializing a node also serializes each of
    // its edges, and if we are traversing a given edge, we must have already
    // visited and serialized the origin node and its edges.
    if (!first)
      return true;

    const JS::ubi::Node& referent = edge.referent;

    if (!zones)
      // We aren't targeting a particular set of zones, so serialize all the
      // things!
      return writer.writeNode(referent, CoreDumpWriter::INCLUDE_EDGES);

    // We are targeting a particular set of zones. If this node is in our target
    // set, serialize it and all of its edges. If this node is _not_ in our
    // target set, we also serialize under the assumption that it is a shared
    // resource being used by something in our target zones since we reached it
    // by traversing the heap graph. However, we do not serialize its outgoing
    // edges and we abandon further traversal from this node.

    JS::Zone* zone = referent.zone();

    if (zones->has(zone))
      return writer.writeNode(referent, CoreDumpWriter::INCLUDE_EDGES);

    traversal.abandonReferent();
    return writer.writeNode(referent, CoreDumpWriter::EXCLUDE_EDGES);
  }
};


bool
WriteHeapGraph(JSContext* cx,
               const JS::ubi::Node& node,
               CoreDumpWriter& writer,
               bool wantNames,
               JS::ZoneSet* zones,
               JS::AutoCheckCannotGC& noGC)
{
  // Serialize the starting node to the core dump.

  if (NS_WARN_IF(!writer.writeNode(node, CoreDumpWriter::INCLUDE_EDGES))) {
    return false;
  }

  // Walk the heap graph starting from the given node and serialize it into the
  // core dump.

  HeapSnapshotHandler handler(writer, zones);
  HeapSnapshotHandler::Traversal traversal(cx, handler, noGC);
  if (!traversal.init())
    return false;
  traversal.wantNames = wantNames;

  return traversal.addStartVisited(node) &&
    traversal.traverse();
}

/* static */ void
ChromeUtils::SaveHeapSnapshot(GlobalObject& global,
                              JSContext* cx,
                              const nsAString& filePath,
                              const HeapSnapshotBoundaries& boundaries,
                              ErrorResult& rv)
{
  bool wantNames = true;
  ZoneSet zones;
  Maybe<AutoCheckCannotGC> maybeNoGC;
  ubi::RootList rootList(cx, maybeNoGC, wantNames);
  if (!EstablishBoundaries(cx, rv, boundaries, rootList, zones))
    return;

  MOZ_ASSERT(maybeNoGC.isSome());
  ubi::Node roots(&rootList);

  nsCOMPtr<nsIFile> file;
  rv = NS_NewLocalFile(filePath, false, getter_AddRefs(file));
  if (NS_WARN_IF(rv.Failed()))
    return;

  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), file,
                                   PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                   -1, 0);
  if (NS_WARN_IF(rv.Failed()))
    return;

  ZeroCopyNSIOutputStream zeroCopyStream(outputStream);
  ::google::protobuf::io::GzipOutputStream gzipStream(&zeroCopyStream);

  StreamWriter writer(cx, gzipStream, wantNames);

  // Serialize the initial heap snapshot metadata to the core dump.
  if (!writer.writeMetadata(PR_Now()) ||
      // Serialize the heap graph to the core dump, starting from our list of
      // roots.
      !WriteHeapGraph(cx,
                      roots,
                      writer,
                      wantNames,
                      zones.initialized() ? &zones : nullptr,
                      maybeNoGC.ref()))
    {
      rv.Throw(zeroCopyStream.failed()
               ? zeroCopyStream.result()
               : NS_ERROR_UNEXPECTED);
      return;
    }
}

/* static */ already_AddRefed<HeapSnapshot>
ChromeUtils::ReadHeapSnapshot(GlobalObject& global,
                              JSContext* cx,
                              const nsAString& filePath,
                              ErrorResult& rv)
{
  UniquePtr<char[]> path(ToNewCString(filePath));
  if (!path) {
    rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  PRFileInfo fileInfo;
  if (PR_GetFileInfo(path.get(), &fileInfo) != PR_SUCCESS) {
    rv.Throw(NS_ERROR_FILE_NOT_FOUND);
    return nullptr;
  }

  uint32_t size = fileInfo.size;
  ScopedFreePtr<uint8_t> buffer(static_cast<uint8_t*>(malloc(size)));
  if (!buffer) {
    rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  PRFileDesc* fd = PR_Open(path.get(), PR_RDONLY, 0);
  if (!fd) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  uint32_t bytesRead = 0;
  while (bytesRead < size) {
    uint32_t bytesLeft = size - bytesRead;
    int32_t bytesReadThisTime = PR_Read(fd, buffer.get() + bytesRead, bytesLeft);
    if (bytesReadThisTime < 1) {
      rv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    bytesRead += bytesReadThisTime;
  }

  return HeapSnapshot::Create(cx, global, buffer.get(), size, rv);
}

} // namespace devtools
} // namespace mozilla
