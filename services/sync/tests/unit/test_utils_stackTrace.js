_("Define some functions in well defined line positions for the test");
function foo(v) bar(v + 1); // line 2
function bar(v) baz(v + 1); // line 3
function baz(v) { throw new Error(v + 1); } // line 4

_("Make sure lazy constructor calling/assignment works");
Cu.import("resource://services-sync/util.js");

function run_test() {
  _("Make sure functions, arguments, files are pretty printed in the trace");
  let trace = "";
  try {
    foo(0);
  }
  catch(ex) {
    trace = Utils.stackTrace(ex);
  }
  _("Got trace:", trace);
  do_check_neq(trace, "");

  let bazPos = trace.indexOf("baz(2)@test_utils_stackTrace.js:4");
  let barPos = trace.indexOf("bar(1)@test_utils_stackTrace.js:3");
  let fooPos = trace.indexOf("foo(0)@test_utils_stackTrace.js:2");
  _("String positions:", bazPos, barPos, fooPos);

  _("Make sure the desired messages show up");
  do_check_true(bazPos >= 0);
  do_check_true(barPos > bazPos);
  do_check_true(fooPos > barPos);
}
