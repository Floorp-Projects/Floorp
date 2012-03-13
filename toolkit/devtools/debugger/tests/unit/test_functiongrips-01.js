/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  gDebuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestGlobalClientAndResume(gClient, "test-grips", function(aResponse, aThreadClient) {
      gThreadClient = aThreadClient;
      test_named_function();
    });
  });
  do_test_pending();
}

function test_named_function()
{
  gThreadClient.addOneTimeListener("paused", function(aEvent, aPacket) {
    let args = aPacket.frame.arguments;

    do_check_eq(args[0].class, "Function");
    // No name for an anonymous function.

    let objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getSignature(function(aResponse) {
      do_check_eq(aResponse.name, "stopMe");
      do_check_eq(aResponse.parameters.length, 1);
      do_check_eq(aResponse.parameters[0], "arg1");

      gThreadClient.resume(function() {
        test_anon_function();
      });
    });

  });

  gDebuggee.eval("stopMe(stopMe)");
}

function test_anon_function() {
  gThreadClient.addOneTimeListener("paused", function(aEvent, aPacket) {
    let args = aPacket.frame.arguments;

    do_check_eq(args[0].class, "Function");
    // No name for an anonymous function.

    let objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getSignature(function(aResponse) {
      do_check_eq(aResponse.name, null);
      do_check_eq(aResponse.parameters.length, 3);
      do_check_eq(aResponse.parameters[0], "foo");
      do_check_eq(aResponse.parameters[1], "bar");
      do_check_eq(aResponse.parameters[2], "baz");

      gThreadClient.resume(function() {
        finishClient(gClient);
      });
    });
  });

  gDebuggee.eval("stopMe(function(foo, bar, baz) { })");
}

