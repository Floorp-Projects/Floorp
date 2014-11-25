/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that setting a breakpoint in a not-yet-existing script doesn't throw
 * an error (see bug 897567). Also make sure that this breakpoint works.
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
  gDebuggee.console = { log: x => void x };
  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect(function () {
    attachTestTabAndResume(gClient,
                           "test-breakpoints",
                           function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      testBreakpoint();
    });
  });
}

const URL = "test.js";

function setUpCode() {
  Cu.evalInSandbox(
    "" + function test() { // 1
      var a = 1;           // 2
      debugger;            // 3
    } +                    // 4
    "\ndebugger;",         // 5
    gDebuggee,
    "1.8",
    URL
  );
}

const testBreakpoint = Task.async(function* () {
  let source = yield getSource(gThreadClient, URL);
  let [response, bpClient] = yield setBreakpoint(source, {line: 2});
  ok(!response.error);

  let actor = response.actor;
  ok(actor);

  yield executeOnNextTickAndWaitForPause(setUpCode, gClient);
  yield resume(gThreadClient);

  let packet = yield executeOnNextTickAndWaitForPause(gDebuggee.test, gClient);
  equal(packet.why.type, "breakpoint")
  notEqual(packet.why.actors.indexOf(actor), -1);

  finishClient(gClient);
});
