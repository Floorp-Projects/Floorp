/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that heap snapshots cross zone boundaries when expected.

#include "DevTools.h"

DEF_TEST(DoesCrossZoneBoundaries, {
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

    // Our set of target zones is both the old and new zones.
    JS::ZoneSet targetZones;
    ASSERT_TRUE(targetZones.init());
    ASSERT_TRUE(targetZones.put(zone));
    ASSERT_TRUE(targetZones.put(newZone));

    FakeNode nodeA(cx);
    FakeNode nodeB(cx);
    FakeNode nodeC(cx);
    FakeNode nodeD(cx);

    nodeA.zone = zone;
    nodeB.zone = nullptr;
    nodeC.zone = newZone;
    nodeD.zone = nullptr;

    AddEdge(nodeA, nodeB);
    AddEdge(nodeA, nodeC);
    AddEdge(nodeB, nodeD);

    ::testing::NiceMock<MockWriter> writer;

    // Should serialize nodeA, because it is in one of our target zones.
    ExpectWriteNode(writer, nodeA);

    // Should serialize nodeB, because it doesn't belong to a zone and is
    // therefore assumed to be shared.
    ExpectWriteNode(writer, nodeB);

    // Should also serialize nodeC, which is in our target zones, but a
    // different zone than A.
    ExpectWriteNode(writer, nodeC);

    // However, should not serialize nodeD because nodeB doesn't belong to one
    // of our target zones and so its edges are excluded from serialization.

    JS::AutoCheckCannotGC noGC(rt);

    ASSERT_TRUE(WriteHeapGraph(cx,
                               JS::ubi::Node(&nodeA),
                               writer,
                               /* wantNames = */ false,
                               &targetZones,
                               noGC));
  });
