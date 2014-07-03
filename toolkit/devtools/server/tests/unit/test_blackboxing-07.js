/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that sources whose URL ends with ".min.js" automatically get black
 * boxed.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-black-box");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestTabAndResume(gClient, "test-black-box", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      testBlackBox();
    });
  });
  do_test_pending();
}

const BLACK_BOXED_URL = "http://example.com/black-boxed.min.js";
const SOURCE_URL = "http://example.com/source.js";

const testBlackBox = Task.async(function* () {
  yield executeOnNextTickAndWaitForPause(evalCode, gClient);

  const { sources } = yield getSources(gThreadClient);
  equal(sources.length, 2);

  const blackBoxedSource = sources.filter(s => s.url === BLACK_BOXED_URL)[0];
  equal(blackBoxedSource.isBlackBoxed, true);

  const regularSource = sources.filter(s => s.url === SOURCE_URL)[0];
  equal(regularSource.isBlackBoxed, false);

  finishClient(gClient);
});

function evalCode() {
  Components.utils.evalInSandbox(
    "" + function blackBoxed() {},
    gDebuggee,
    "1.8",
    BLACK_BOXED_URL,
    1
  );

  Components.utils.evalInSandbox(
    "" + function source() {}
      + "\ndebugger;",
    gDebuggee,
    "1.8",
    SOURCE_URL,
    1
  );
}
