/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that when we add 2 breakpoints to the same line at different columns and
 * then remove one of them, we don't remove them both.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-breakpoints");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestTabAndResume(gClient, "test-breakpoints", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_breakpoints_columns();
    });
  });
  do_test_pending();
}

const URL = "http://example.com/benderbendingrodriguez.js";

const code =
"(" + function (global) {
  global.foo = function () {
    Math.abs(-1); Math.log(0.5);
    debugger;
  };
  debugger;
} + "(this))";

const firstLocation = {
  url: URL,
  line: 3,
  column: 4
};

const secondLocation = {
  url: URL,
  line: 3,
  column: 18
};

function test_breakpoints_columns() {
  gClient.addOneTimeListener("paused", set_breakpoints);

  Components.utils.evalInSandbox(code, gDebuggee, "1.8", URL, 1);
}

function set_breakpoints() {
  let first, second;

  gThreadClient.setBreakpoint(firstLocation, function ({ error, actualLocation },
                                                       aBreakpointClient) {
    do_check_true(!error, "Should not get an error setting the breakpoint");
    do_check_true(!actualLocation, "Should not get an actualLocation");
    first = aBreakpointClient;

    gThreadClient.setBreakpoint(secondLocation, function ({ error, actualLocation },
                                                          aBreakpointClient) {
      do_check_true(!error, "Should not get an error setting the breakpoint");
      do_check_true(!actualLocation, "Should not get an actualLocation");
      second = aBreakpointClient;

      test_different_actors(first, second);
    });
  });
}

function test_different_actors(aFirst, aSecond) {
  do_check_neq(aFirst.actor, aSecond.actor,
               "Each breakpoint should have a different actor");
  test_remove_one(aFirst, aSecond);
}

function test_remove_one(aFirst, aSecond) {
  aFirst.remove(function ({error}) {
    do_check_true(!error, "Should not get an error removing a breakpoint");

    let hitSecond;
    gClient.addListener("paused", function _onPaused(aEvent, {why, frame}) {
      if (why.type == "breakpoint") {
        hitSecond = true;
        do_check_eq(why.actors.length, 1,
                    "Should only be paused because of one breakpoint actor");
        do_check_eq(why.actors[0], aSecond.actor,
                    "Should be paused because of the correct breakpoint actor");
        do_check_eq(frame.where.line, secondLocation.line,
                    "Should be at the right line");
        do_check_eq(frame.where.column, secondLocation.column,
                    "Should be at the right column");
        return void gThreadClient.resume();
      }

      if (why.type == "debuggerStatement") {
        gClient.removeListener("paused", _onPaused);
        do_check_true(hitSecond,
                      "We should still hit `second`, but not `first`.");

        return void finishClient(gClient);
      }

      do_check_true(false, "Should never get here");
    });

    gThreadClient.resume(() => gDebuggee.foo());
  });
}
