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
  gDebuggee.noop = x => x;
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestTabAndResume(gClient, "test-pretty-print", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      evalCode();
    });
  });
  do_test_pending();
}

const CODE = "" + function main() { var a = 1; debugger; noop(a); return 10; };
const CODE_URL = "data:text/javascript," + CODE;

const BP_LOCATION = {
  url: CODE_URL,
  line: 5,
  column: 2
};

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

function testDbgStatement(event, { why, frame }) {
  do_check_eq(why.type, "debuggerStatement");
  const { url, line, column } = frame.where;
  do_check_eq(url, CODE_URL);
  do_check_eq(line, 3);
  setBreakpoint();
}

function setBreakpoint() {
  gThreadClient.setBreakpoint(BP_LOCATION, ({ error, actualLocation }) => {
    do_check_true(!error);
    do_check_true(!actualLocation);
    testStepping();
  });
}

function testStepping() {
  gClient.addOneTimeListener("paused", (event, { why, frame }) => {
    do_check_eq(why.type, "resumeLimit");
    const { url, line } = frame.where;
    do_check_eq(url, CODE_URL);
    do_check_eq(line, 4);
    testHitBreakpoint();
  });
  gThreadClient.stepIn();
}

function testHitBreakpoint() {
  gClient.addOneTimeListener("paused", (event, { why, frame }) => {
    do_check_eq(why.type, "breakpoint");
    const { url, line, column } = frame.where;
    do_check_eq(url, CODE_URL);
    do_check_eq(line, BP_LOCATION.line);
    do_check_eq(column, BP_LOCATION.column);
    finishClient(gClient);
  });
  gThreadClient.resume();
}
