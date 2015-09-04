/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that heap snapshots walk the zone boundaries correctly.

#include "DevTools.h"

DEF_TEST(DoesntCrossZoneBoundaries, {
    // Create a new global to get a new zone.
    JS::RootedObject newGlobal(cx, JS_NewGlobalObject(cx,
                                                      getGlobalClass(),
                                                      nullptr,
                                                      JS::FireOnNewGlobalHook));
    ASSERT_TRUE(newGlobal);
    JS::Zone* newZone = nullptr;
    {
      JSAutoCompartment ac(cx, newGlobal);
      ASSERT_TRUE(JS_InitStandardClasses(cx, newGlobal));
      newZone = js::GetContextZone(cx);
    }
    ASSERT_TRUE(newZone);
    ASSERT_NE(newZone, zone);

    // Our set of target zones is only the pre-existing zone and does not
    // include the new zone.
    JS::ZoneSet targetZones;
    ASSERT_TRUE(targetZones.init());
    ASSERT_TRUE(targetZones.put(zone));

    FakeNode nodeA(cx);
    FakeNode nodeB(cx);
    FakeNode nodeC(cx);

    nodeA.zone = zone;
    nodeB.zone = nullptr;
    nodeC.zone = newZone;

    AddEdge(nodeA, nodeB);
    AddEdge(nodeB, nodeC);

    ::testing::NiceMock<MockWriter> writer;

    // Should serialize nodeA, because it is in our target zones.
    ExpectWriteNode(writer, nodeA);

    // Should serialize nodeB, because it doesn't belong to a zone and is
    // therefore assumed to be shared.
    ExpectWriteNode(writer, nodeB);

    // But we shouldn't ever serialize nodeC.

    JS::AutoCheckCannotGC noGC(rt);

    ASSERT_TRUE(WriteHeapGraph(cx,
                               JS::ubi::Node(&nodeA),
                               writer,
                               /* wantNames = */ false,
                               &targetZones,
                               noGC));
  });
