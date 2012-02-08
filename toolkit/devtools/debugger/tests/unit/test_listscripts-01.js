/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check basic listScripts functionality.
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
    attachTestGlobalClientAndResume(gClient, "test-stack", function (aResponse, aThreadClient) {
      gThreadClient = aThreadClient;
      test_simple_listscripts();
    });
  });
  do_test_pending();
}

function test_simple_listscripts()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    var path = getFilePath('test_listscripts-01.js');
    gThreadClient.getScripts(function (aResponse) {
        // Check the return value.
        do_check_eq(aResponse.scripts[0].url, path);
        do_check_eq(aResponse.scripts[0].startLine, 41);
        do_check_eq(aResponse.scripts[0].lineCount, 4);
        gThreadClient.resume(function () {
          finishClient(gClient);
        });
    });
  });

  gDebuggee.eval("var line0 = Error().lineNumber;\n" +
       "debugger;\n" +   // line0 + 1
       "var a = 1;\n" +  // line0 + 2
       "var b = 2;\n");  // line0 + 3
}
