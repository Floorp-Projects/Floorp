/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that adding a breakpoint in the same place returns the same actor.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function () {
    attachTestTabAndResume(gClient, "test-stack", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_same_breakpoint();
    });
  });
  do_test_pending();
}

function test_same_breakpoint()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let path = getFilePath('test_breakpoint-01.js');
    let location = {
      url: path,
      line: gDebuggee.line0 + 3
    };
    gThreadClient.setBreakpoint(location, function (aResponse, bpClient) {
      gThreadClient.setBreakpoint(location, function (aResponse, secondBpClient) {
        do_check_eq(bpClient.actor, secondBpClient.actor,
                    "Should get the same actor w/ whole line breakpoints");
        let location = {
          url: path,
          line: gDebuggee.line0 + 2,
          column: 6
        };
        gThreadClient.setBreakpoint(location, function (aResponse, bpClient) {
          gThreadClient.setBreakpoint(location, function (aResponse, secondBpClient) {
            do_check_eq(bpClient.actor, secondBpClient.actor,
                        "Should get the same actor column breakpoints");
            finishClient(gClient);
          });
        });
      });
    });

  });

  Components.utils.evalInSandbox("var line0 = Error().lineNumber;\n" +
                                 "debugger;\n" +   // line0 + 1
                                 "var a = 1;\n" +  // line0 + 2
                                 "var b = 2;\n",   // line0 + 3
                                 gDebuggee);
}
