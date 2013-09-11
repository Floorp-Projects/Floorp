/* -*- Mode: javascript; js-indent-level: 2; -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test pretty printing source mapped sources.

var gDebuggee;
var gClient;
var gThreadClient;
var gSource;

Components.utils.import('resource:///modules/devtools/SourceMap.jsm');

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-pretty-print");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestTabAndResume(gClient, "test-pretty-print", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      evalCode();
    });
  });
  do_test_pending();
}

const dataUrl = s => "data:text/javascript," + s;

const A = "function a(){b()}";
const A_URL = dataUrl(A);
const B = "function b(){debugger}";
const B_URL = dataUrl(B);

function evalCode() {
  let { code, map } = (new SourceNode(null, null, null, [
    new SourceNode(1, 0, A_URL, A),
    B.split("").map((ch, i) => new SourceNode(1, i, B_URL, ch))
  ])).toStringWithSourceMap({
    file: "abc.js"
  });

  code += "//# sourceMappingURL=data:text/json;base64," + btoa(map.toString());

  gClient.addListener("newSource", waitForB);
  Components.utils.evalInSandbox(code, gDebuggee, "1.8",
                                 "http://example.com/foo.js", 1);
}

function waitForB(event, { source }) {
  if (source.url !== B_URL) {
    return;
  }
  gClient.removeListener("newSource", waitForB);
  prettyPrint(source);
}

function prettyPrint(source) {
  gThreadClient.source(source).prettyPrint(2, runCode);
}

function runCode({ error }) {
  do_check_true(!error);
  gClient.addOneTimeListener("paused", testDbgStatement);
  gDebuggee.a();
}

function testDbgStatement(event, { frame, why }) {
  do_check_eq(why.type, "debuggerStatement");
  const { url, line, column } = frame.where;
  do_check_eq(url, B_URL);
  do_check_eq(line, 2);
  do_check_eq(column, 2);
  finishClient(gClient);
}
