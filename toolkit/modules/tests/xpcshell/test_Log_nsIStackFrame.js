const { Log } = ChromeUtils.importESModule(
  "resource://gre/modules/Log.sys.mjs"
);

function foo() {
  return bar(); // line 6
}

function bar() {
  return baz(); // line 10
}

function baz() {
  return Log.stackTrace(Components.stack.caller); // line 14
}

function start() {
  return next(); // line 18
}

function next() {
  return finish(); // line 22
}

function finish() {
  return Log.stackTrace(); // line 26
}

function run_test() {
  const stackFrameTrace = foo(); // line 30

  print(`Got trace for nsIStackFrame case: ${stackFrameTrace}`);

  const fooPos = stackFrameTrace.search(/foo@.*test_Log_nsIStackFrame.js:6/);
  const barPos = stackFrameTrace.search(/bar@.*test_Log_nsIStackFrame.js:10/);
  const runTestPos = stackFrameTrace.search(
    /run_test@.*test_Log_nsIStackFrame.js:30/
  );

  print(`String positions: ${runTestPos} ${barPos} ${fooPos}`);
  Assert.ok(barPos >= 0);
  Assert.ok(fooPos > barPos);
  Assert.ok(runTestPos > fooPos);

  const emptyArgTrace = start();

  print(`Got trace for empty argument case: ${emptyArgTrace}`);

  const startPos = emptyArgTrace.search(/start@.*test_Log_nsIStackFrame.js:18/);
  const nextPos = emptyArgTrace.search(/next@.*test_Log_nsIStackFrame.js:22/);
  const finishPos = emptyArgTrace.search(
    /finish@.*test_Log_nsIStackFrame.js:26/
  );

  print(`String positions: ${finishPos} ${nextPos} ${startPos}`);
  Assert.ok(finishPos >= 0);
  Assert.ok(nextPos > finishPos);
  Assert.ok(startPos > nextPos);
}
