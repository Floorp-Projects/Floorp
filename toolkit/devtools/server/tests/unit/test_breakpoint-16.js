/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that we can set breakpoints in columns, not just lines.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test()
{
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
};

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-breakpoints", aServer);
  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect(function () {
    attachTestTabAndResume(gClient,
                           "test-breakpoints",
                           function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_column_breakpoint();
    });
  });
}

function test_column_breakpoint()
{
  // Debugger statement
  gClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let source = gThreadClient.source(aPacket.frame.where.source);
    let location = {
      line: gDebuggee.line0 + 1,
      column: 55
    };
    let timesBreakpointHit = 0;

    source.setBreakpoint(location, function (aResponse, bpClient) {
      gThreadClient.addListener("paused", function onPaused(aEvent, aPacket) {
        do_check_eq(aPacket.type, "paused");
        do_check_eq(aPacket.why.type, "breakpoint");
        do_check_eq(aPacket.why.actors[0], bpClient.actor);
        do_check_eq(aPacket.frame.where.source.actor, source.actor);
        do_check_eq(aPacket.frame.where.line, location.line);
        do_check_eq(aPacket.frame.where.column, location.column);

        do_check_eq(gDebuggee.acc, timesBreakpointHit);
        do_check_eq(aPacket.frame.environment.bindings.variables.i.value,
                    timesBreakpointHit);

        if (++timesBreakpointHit === 3) {
          gThreadClient.removeListener("paused", onPaused);
          bpClient.remove(function (aResponse) {
            gThreadClient.resume(() => gClient.close(gCallback));
          });
        } else {
          gThreadClient.resume();
        }
      });

      // Continue until the breakpoint is hit.
      gThreadClient.resume();
    });

  });


  Components.utils.evalInSandbox(
    "var line0 = Error().lineNumber;\n" +
    "(function () { debugger; this.acc = 0; for (var i = 0; i < 3; i++) this.acc++; }());",
    gDebuggee
  );
}
