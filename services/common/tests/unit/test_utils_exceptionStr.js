/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  run_next_test();
}

add_test(function test_exceptionStr_non_exceptions() {
  do_check_eq(CommonUtils.exceptionStr(null), "null");
  do_check_eq(CommonUtils.exceptionStr(false), "false");
  do_check_eq(CommonUtils.exceptionStr(undefined), "undefined");
  do_check_eq(CommonUtils.exceptionStr(12), "12 No traceback available");

  run_next_test();
});
