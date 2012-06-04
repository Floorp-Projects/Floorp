/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gClient;
var gDebuggee;

function run_test()
{
  DebuggerServer.addActors("resource://test/testactors.js");

  // Allow incoming connections.
  DebuggerServer.init(function () { return true; });
  gDebuggee = testGlobal("test-1");
  DebuggerServer.addTestGlobal(gDebuggee);

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(function(aType, aTraits) {
    getTestGlobalContext(gClient, "test-1", function(aContext) {
      test_attach(aContext);
    });
  });
  do_test_pending();
}

function test_attach(aContext)
{
  gClient.attachThread(aContext.actor, function(aResponse, aThreadClient) {
    do_check_eq(aThreadClient.state, "paused");
    aThreadClient.resume(function() {
      cleanup();
    });
  });
}

function cleanup()
{
  gClient.addListener("closed", function(aEvent) {
    do_test_finished();
  });
  gClient.close();
}

