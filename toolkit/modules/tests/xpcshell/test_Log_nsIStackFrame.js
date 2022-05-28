const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");

function foo() {
  return bar(); // line 4
}

function bar() {
  return baz(); // line 8
}

function baz() {
  return Log.stackTrace(Components.stack.caller); // line 12
}

function start() {
  return next(); // line 16
}

function next() {
  return finish(); // line 20
}

function finish() {
  return Log.stackTrace(); // line 24
}

function run_test() {
  const stackFrameTrace = foo(); // line 28

  print(`Got trace for nsIStackFrame case: ${stackFrameTrace}`);

  const fooPos = stackFrameTrace.search(/foo@.*test_Log_nsIStackFrame.js:4/);
  const barPos = stackFrameTrace.search(/bar@.*test_Log_nsIStackFrame.js:8/);
  const runTestPos = stackFrameTrace.search(
    /run_test@.*test_Log_nsIStackFrame.js:28/
  );

  print(`String positions: ${runTestPos} ${barPos} ${fooPos}`);
  Assert.ok(barPos >= 0);
  Assert.ok(fooPos > barPos);
  Assert.ok(runTestPos > fooPos);

  const emptyArgTrace = start();

  print(`Got trace for empty argument case: ${emptyArgTrace}`);

  const startPos = emptyArgTrace.search(/start@.*test_Log_nsIStackFrame.js:16/);
  const nextPos = emptyArgTrace.search(/next@.*test_Log_nsIStackFrame.js:20/);
  const finishPos = emptyArgTrace.search(
    /finish@.*test_Log_nsIStackFrame.js:24/
  );

  print(`String positions: ${finishPos} ${nextPos} ${startPos}`);
  Assert.ok(finishPos >= 0);
  Assert.ok(nextPos > finishPos);
  Assert.ok(startPos > nextPos);
}
