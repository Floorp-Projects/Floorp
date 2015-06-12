/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_ChromeUtils__
#define mozilla_devtools_ChromeUtils__

#include "CoreDump.pb.h"
#include "jsapi.h"
#include "jsfriendapi.h"

#include "js/UbiNode.h"
#include "js/UbiNodeTraverse.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ChromeUtilsBinding.h"

namespace mozilla {
namespace devtools {

// A `CoreDumpWriter` is given the data we wish to save in a core dump and
// serializes it to disk, or memory, or a socket, etc.
class CoreDumpWriter
{
public:
  virtual ~CoreDumpWriter() { };

  // Write the given bits of metadata we would like to associate with this core
  // dump.
  virtual bool writeMetadata(uint64_t timestamp) = 0;

  enum EdgePolicy : bool {
    INCLUDE_EDGES = true,
    EXCLUDE_EDGES = false
  };

  // Write the given `JS::ubi::Node` to the core dump. The given `EdgePolicy`
  // dictates whether its outgoing edges should also be written to the core
  // dump, or excluded.
  virtual bool writeNode(const JS::ubi::Node& node,
                         EdgePolicy includeEdges) = 0;
};


// Serialize the heap graph as seen from `node` with the given
// `CoreDumpWriter`. If `wantNames` is true, capture edge names. If `zones` is
// non-null, only capture the sub-graph within the zone set, otherwise capture
// the whole heap graph. Returns false on failure.
bool
WriteHeapGraph(JSContext* cx,
               const JS::ubi::Node& node,
               CoreDumpWriter& writer,
               bool wantNames,
               JS::ZoneSet* zones,
               JS::AutoCheckCannotGC& noGC);


class HeapSnapshot;


class ChromeUtils
{
public:
  static void SaveHeapSnapshot(dom::GlobalObject& global,
                               JSContext* cx,
                               const nsAString& filePath,
                               const dom::HeapSnapshotBoundaries& boundaries,
                               ErrorResult& rv);

  static already_AddRefed<HeapSnapshot> ReadHeapSnapshot(dom::GlobalObject& global,
                                                         JSContext* cx,
                                                         const nsAString& filePath,
                                                         ErrorResult& rv);

  static void
  OriginAttributesToCookieJar(dom::GlobalObject& aGlobal,
                              const dom::OriginAttributesDictionary& aAttrs,
                              nsCString& aCookieJar);
};

} // namespace devtools
} // namespace mozilla

#endif // mozilla_devtools_ChromeUtils__
