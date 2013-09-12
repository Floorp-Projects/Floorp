/* -*- Mode: javascript; js-indent-level: 2; -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gDebuggee;
var gClient;
var gThreadClient;
var gSource;

// Test stepping through pretty printed sources.

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

const CODE = "" + function main() { debugger; return 10; };
const CODE_URL = "data:text/javascript," + CODE;

function evalCode() {
  gClient.addOneTimeListener("newSource", prettyPrintSource);
  Cu.evalInSandbox(
    CODE,
    gDebuggee,
    "1.8",
    CODE_URL,
    1
  );
}

function prettyPrintSource(event, { source }) {
  gSource = source;
  gThreadClient.source(gSource).prettyPrint(2, runCode);
}

function runCode({ error }) {
  do_check_true(!error);
  gClient.addOneTimeListener("paused", testDbgStatement);
  gDebuggee.main();
}

function testDbgStatement(event, { frame }) {
  const { url, line, column } = frame.where;
  do_check_eq(url, CODE_URL);
  do_check_eq(line, 2);
  do_check_eq(column, 2);
  testStepping();
}

function testStepping() {
  gClient.addOneTimeListener("paused", (event, { frame }) => {
    const { url, line, column } = frame.where;
    do_check_eq(url, CODE_URL);
    do_check_eq(line, 3);
    do_check_eq(column, 2);
    finishClient(gClient);
  });
  gThreadClient.stepIn();
}
