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
    attachTestTabAndResume(gClient, "test-grips", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_object_grip();
    });
  });
  do_test_pending();
}

function test_object_grip()
{
  gThreadClient.addOneTimeListener("paused", function(aEvent, aPacket) {
    let args = aPacket.frame.arguments;

    do_check_eq(args[0].class, "Object");

    let objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getPrototypeAndProperties(function(aResponse) {
      do_check_eq(aResponse.ownProperties.a.configurable, true);
      do_check_eq(aResponse.ownProperties.a.enumerable, true);
      do_check_eq(aResponse.ownProperties.a.writable, true);
      do_check_eq(aResponse.ownProperties.a.value.type, "Infinity");

      do_check_eq(aResponse.ownProperties.b.configurable, true);
      do_check_eq(aResponse.ownProperties.b.enumerable, true);
      do_check_eq(aResponse.ownProperties.b.writable, true);
      do_check_eq(aResponse.ownProperties.b.value.type, "-Infinity");

      do_check_eq(aResponse.ownProperties.c.configurable, true);
      do_check_eq(aResponse.ownProperties.c.enumerable, true);
      do_check_eq(aResponse.ownProperties.c.writable, true);
      do_check_eq(aResponse.ownProperties.c.value.type, "NaN");

      do_check_eq(aResponse.ownProperties.d.configurable, true);
      do_check_eq(aResponse.ownProperties.d.enumerable, true);
      do_check_eq(aResponse.ownProperties.d.writable, true);
      do_check_eq(aResponse.ownProperties.d.value.type, "-0");

      gThreadClient.resume(function() {
        finishClient(gClient);
      });
    });
  });

  gDebuggee.eval("stopMe({ a: Infinity, b: -Infinity, c: NaN, d: -0 })");
}

