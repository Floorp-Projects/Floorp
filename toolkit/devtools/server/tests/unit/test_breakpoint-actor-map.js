/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the functionality of the BreakpointActorMap object.

const { BreakpointActorMap, ThreadActor } = devtools.require("devtools/server/actors/script");

function run_test()
{
  Cu.import("resource://gre/modules/jsdebugger.jsm");
  addDebuggerToGlobal(this);

  test_get_actor();
  test_set_actor();
  test_delete_actor();
  test_find_actors();
  test_duplicate_actors();
}

function test_get_actor() {
  let bpStore = new BreakpointActorMap();
  let location = {
    sourceActor: { actor: 'actor1' },
    line: 3
  };
  let columnLocation = {
    sourceActor: { actor: 'actor2' },
    line: 5,
    column: 15
  };

  // Shouldn't have breakpoint
  do_check_eq(null, bpStore.getActor(location),
              "Breakpoint not added and shouldn't exist.");

  bpStore.setActor(location, {});
  do_check_true(!!bpStore.getActor(location),
                "Breakpoint added but not found in Breakpoint Store.");

  bpStore.deleteActor(location);
  do_check_eq(null, bpStore.getActor(location),
              "Breakpoint removed but still exists.");

  // Same checks for breakpoint with a column
  do_check_eq(null, bpStore.getActor(columnLocation),
              "Breakpoint with column not added and shouldn't exist.");

  bpStore.setActor(columnLocation, {});
  do_check_true(!!bpStore.getActor(columnLocation),
                "Breakpoint with column added but not found in Breakpoint Store.");

  bpStore.deleteActor(columnLocation);
  do_check_eq(null, bpStore.getActor(columnLocation),
              "Breakpoint with column removed but still exists in Breakpoint Store.");
}

function test_set_actor() {
  // Breakpoint with column
  let bpStore = new BreakpointActorMap();
  let location = {
    sourceActor: { actor: 'actor1' },
    line: 10,
    column: 9
  };
  bpStore.setActor(location, {});
  do_check_true(!!bpStore.getActor(location),
                "We should have the column breakpoint we just added");

  // Breakpoint without column (whole line breakpoint)
  location = {
    sourceActor: { actor: 'actor2' },
    line: 103
  };
  bpStore.setActor(location, {});
  do_check_true(!!bpStore.getActor(location),
                "We should have the whole line breakpoint we just added");
}

function test_delete_actor() {
  // Breakpoint with column
  let bpStore = new BreakpointActorMap();
  let location = {
    sourceActor: { actor: 'actor1' },
    line: 10,
    column: 9
  };
  bpStore.setActor(location, {});
  bpStore.deleteActor(location);
  do_check_eq(bpStore.getActor(location), null,
              "We should not have the column breakpoint anymore");

  // Breakpoint without column (whole line breakpoint)
  location = {
    sourceActor: { actor: 'actor2' },
    line: 103
  };
  bpStore.setActor(location, {});
  bpStore.deleteActor(location);
  do_check_eq(bpStore.getActor(location), null,
              "We should not have the whole line breakpoint anymore");
}

function test_find_actors() {
  let bps = [
    { sourceActor: { actor: "actor1" }, line: 10 },
    { sourceActor: { actor: "actor1" }, line: 10, column: 3 },
    { sourceActor: { actor: "actor1" }, line: 10, column: 10 },
    { sourceActor: { actor: "actor1" }, line: 23, column: 89 },
    { sourceActor: { actor: "actor2" }, line: 10, column: 1 },
    { sourceActor: { actor: "actor2" }, line: 20, column: 5 },
    { sourceActor: { actor: "actor2" }, line: 30, column: 34 },
    { sourceActor: { actor: "actor2" }, line: 40, column: 56 }
  ];

  let bpStore = new BreakpointActorMap();

  for (let bp of bps) {
    bpStore.setActor(bp, bp);
  }

  // All breakpoints

  let bpSet = new Set(bps);
  for (let bp of bpStore.findActors()) {
    bpSet.delete(bp);
  }
  do_check_eq(bpSet.size, 0,
              "Should be able to iterate over all breakpoints");

  // Breakpoints by URL

  bpSet = new Set(bps.filter(bp => { return bp.sourceActor.actorID === "actor1" }));
  for (let bp of bpStore.findActors({ sourceActor: { actorID: "actor1" } })) {
    bpSet.delete(bp);
  }
  do_check_eq(bpSet.size, 0,
              "Should be able to filter the iteration by url");

  // Breakpoints by URL and line

  bpSet = new Set(bps.filter(bp => { return bp.sourceActor.actorID === "actor1" && bp.line === 10; }));
  let first = true;
  for (let bp of bpStore.findActors({ sourceActor: { actorID: "actor1" }, line: 10 })) {
    if (first) {
      do_check_eq(bp.column, undefined,
                  "Should always get the whole line breakpoint first");
      first = false;
    } else {
      do_check_neq(bp.column, undefined,
                   "Should not get the whole line breakpoint any time other than first.");
    }
    bpSet.delete(bp);
  }
  do_check_eq(bpSet.size, 0,
              "Should be able to filter the iteration by url and line");
}

function test_duplicate_actors() {
  let bpStore = new BreakpointActorMap();

  // Breakpoint with column
  let location = {
    sourceActor: { actorID: "foo-actor" },
    line: 10,
    column: 9
  };
  bpStore.setActor(location, {});
  bpStore.setActor(location, {});
  do_check_eq(bpStore.size, 1, "We should have only 1 column breakpoint");
  bpStore.deleteActor(location);

  // Breakpoint without column (whole line breakpoint)
  location = {
    sourceActor: { actorID: "foo-actor" },
    line: 15
  };
  bpStore.setActor(location, {});
  bpStore.setActor(location, {});
  do_check_eq(bpStore.size, 1, "We should have only 1 whole line breakpoint");
  bpStore.deleteActor(location);
}
