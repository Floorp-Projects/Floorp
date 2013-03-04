/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check basic newScript packet sent from server.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestGlobalClientAndResume(gClient, "test-stack", function(aResponse, aThreadClient) {
      gThreadClient = aThreadClient;
      test_simple_new_script();
    });
  });
  do_test_pending();
}

function test_simple_new_script()
{
  gClient.addOneTimeListener("newScript", function (aEvent, aPacket) {
    do_check_eq(aEvent, "newScript");
    do_check_eq(aPacket.type, "newScript");
    do_check_true(!!aPacket.url.match(/test_new_script-01.js$/));
    do_check_true(!!aPacket.source);
    do_check_true(!!aPacket.source.url.match(/test_new_script-01.js$/));
    do_check_eq(aPacket.startLine, gDebuggee.line0);

    finishClient(gClient);
  });

  gDebuggee.eval("var line0 = Error().lineNumber;");
}
