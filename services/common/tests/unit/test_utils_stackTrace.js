/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

_("Define some functions in well defined line positions for the test");
function foo(v) bar(v + 1); // line 2
function bar(v) baz(v + 1); // line 3
function baz(v) { throw new Error(v + 1); } // line 4

_("Make sure lazy constructor calling/assignment works");
Cu.import("resource://services-common/utils.js");

function run_test() {
  _("Make sure functions, arguments, files are pretty printed in the trace");
  let trace = "";
  try {
    foo(0);
  }
  catch(ex) {
    trace = CommonUtils.stackTrace(ex);
  }
  _("Got trace:", trace);
  do_check_neq(trace, "");

  let bazPos = trace.indexOf("baz@test_utils_stackTrace.js:7");
  let barPos = trace.indexOf("bar@test_utils_stackTrace.js:6");
  let fooPos = trace.indexOf("foo@test_utils_stackTrace.js:5");
  _("String positions:", bazPos, barPos, fooPos);

  _("Make sure the desired messages show up");
  do_check_true(bazPos >= 0);
  do_check_true(barPos > bazPos);
  do_check_true(fooPos > barPos);
}
