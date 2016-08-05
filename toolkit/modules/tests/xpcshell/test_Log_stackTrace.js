print("Define some functions in well defined line positions for the test");
function foo(v) { return bar(v + 1); } // line 2
function bar(v) { return baz(v + 1); } // line 3
function baz(v) { throw new Error(v + 1); } // line 4

print("Make sure lazy constructor calling/assignment works");
Components.utils.import("resource://gre/modules/Log.jsm");

function run_test() {
  print("Make sure functions, arguments, files are pretty printed in the trace");
  let trace = "";
  try {
    foo(0);
  }
  catch (ex) {
    trace = Log.stackTrace(ex);
  }
  print(`Got trace: ${trace}`);
  do_check_neq(trace, "");

  let bazPos = trace.indexOf("baz@test_Log_stackTrace.js:4");
  let barPos = trace.indexOf("bar@test_Log_stackTrace.js:3");
  let fooPos = trace.indexOf("foo@test_Log_stackTrace.js:2");
  print(`String positions: ${bazPos} ${barPos} ${fooPos}`);

  print("Make sure the desired messages show up");
  do_check_true(bazPos >= 0);
  do_check_true(barPos > bazPos);
  do_check_true(fooPos > barPos);
}
