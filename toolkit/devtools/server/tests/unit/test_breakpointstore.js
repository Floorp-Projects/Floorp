/* -*- Mode: js; js-indent-level: 2; -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the functionality of the BreakpointStore object.

function run_test()
{
  Cu.import("resource://gre/modules/jsdebugger.jsm");
  addDebuggerToGlobal(this);
  let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Components.interfaces.mozIJSSubScriptLoader);
  loader.loadSubScript("resource://gre/modules/devtools/server/actors/script.js");

  test_bug_754251();
  test_add_breakpoint();
  test_remove_breakpoint();
  test_find_breakpoints();
}

// Note: Removing this test will regress bug 754251. See comment above
// ThreadActor._breakpointStore.
function test_bug_754251() {
  let instance1 = new ThreadActor();
  let instance2 = new ThreadActor();
  do_check_true(instance1.breakpointStore instanceof BreakpointStore);
  do_check_eq(instance1.breakpointStore, ThreadActor.breakpointStore);
  do_check_eq(instance2.breakpointStore, ThreadActor.breakpointStore);
}

function test_add_breakpoint() {
  // Breakpoint with column
  let bpStore = new BreakpointStore();
  let location = {
    url: "http://example.com/foo.js",
    line: 10,
    column: 9
  };
  bpStore.addBreakpoint(location);
  do_check_true(!!bpStore.getBreakpoint(location, false),
                "We should have the column breakpoint we just added");

  // Breakpoint without column (whole line breakpoint)
  location = {
    url: "http://example.com/bar.js",
    line: 103
  };
  bpStore.addBreakpoint(location);
  do_check_true(!!bpStore.getBreakpoint(location, false),
                "We should have the whole line breakpoint we just added");
}

function test_remove_breakpoint() {
  // Breakpoint with column
  let bpStore = new BreakpointStore();
  let location = {
    url: "http://example.com/foo.js",
    line: 10,
    column: 9
  };
  bpStore.addBreakpoint(location);
  bpStore.removeBreakpoint(location);
  do_check_eq(bpStore.getBreakpoint(location, false), null,
              "We should not have the column breakpoint anymore");

  // Breakpoint without column (whole line breakpoint)
  location = {
    url: "http://example.com/bar.js",
    line: 103
  };
  bpStore.addBreakpoint(location);
  bpStore.removeBreakpoint(location);
  do_check_eq(bpStore.getBreakpoint(location, false), null,
              "We should not have the whole line breakpoint anymore");
}

function test_find_breakpoints() {
  let bps = [
    { url: "foo.js", line: 10 },
    { url: "foo.js", line: 10, column: 3 },
    { url: "foo.js", line: 10, column: 10 },
    { url: "foo.js", line: 23, column: 89 },
    { url: "bar.js", line: 10, column: 1 },
    { url: "bar.js", line: 20, column: 5 },
    { url: "bar.js", line: 30, column: 34 },
    { url: "bar.js", line: 40, column: 56 }
  ];

  let bpStore = new BreakpointStore();

  for (let bp of bps) {
    bpStore.addBreakpoint(bp);
  }

  // All breakpoints

  let bpSet = Set(bps);
  for (let bp of bpStore.findBreakpoints()) {
    bpSet.delete(bp);
  }
  do_check_eq(bpSet.size, 0,
              "Should be able to iterate over all breakpoints");

  // Breakpoints by URL

  bpSet = Set(bps.filter(bp => { return bp.url === "foo.js" }));
  for (let bp of bpStore.findBreakpoints({ url: "foo.js" })) {
    bpSet.delete(bp);
  }
  do_check_eq(bpSet.size, 0,
              "Should be able to filter the iteration by url");

  // Breakpoints by URL and line

  bpSet = Set(bps.filter(bp => { return bp.url === "foo.js" && bp.line === 10; }));
  let first = true;
  for (let bp of bpStore.findBreakpoints({ url: "foo.js", line: 10 })) {
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
