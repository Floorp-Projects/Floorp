/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
  gDebuggee = addTestGlobal("test-grips", aServer);
  gDebuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect(function() {
    attachTestTabAndResume(gClient, "test-grips", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_object_grip();
    });
  });
}

function test_object_grip()
{
  gThreadClient.addOneTimeListener("paused", function(aEvent, aPacket) {
    let args = aPacket.frame.arguments;

    do_check_eq(args[0].class, "Object");

    let objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getPrototype(function(aResponse) {
      do_check_true(aResponse.prototype != undefined);

      let protoClient = gThreadClient.pauseGrip(aResponse.prototype);
      protoClient.getOwnPropertyNames(function(aResponse) {
        do_check_eq(aResponse.ownPropertyNames.length, 2);
        do_check_eq(aResponse.ownPropertyNames[0], "b");
        do_check_eq(aResponse.ownPropertyNames[1], "c");

        gThreadClient.resume(function() {
          gClient.close(gCallback);
        });
      });
    });

  });

  gDebuggee.eval(function Constr() {
    this.a = 1;
  }.toString());
  gDebuggee.eval("Constr.prototype = { b: true, c: 'foo' }; var o = new Constr(); stopMe(o)");
}

