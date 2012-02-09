/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test()
{
  test_nesting();
}

function test_nesting()
{
  let inspector = Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);
  do_check_eq(inspector.eventLoopNestLevel, 0);

  let tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
  tm.currentThread.dispatch({ run: function() {
    do_check_eq(inspector.eventLoopNestLevel, 1);
    do_check_eq(inspector.exitNestedEventLoop(), 0);
  }}, 0);

  do_check_eq(inspector.enterNestedEventLoop(), 0);
  do_check_eq(inspector.eventLoopNestLevel, 0);
}