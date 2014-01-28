/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");

function run_test() {
  do_test_pending();
  run_next_test();
}

add_task(function test_typeerror() {
  let exn;
  try {
    let fd = yield OS.File.open("/tmp", {no_such_key: 1});
    do_print("Fd: " + fd);
  } catch (ex) {
    exn = ex;
  }
  do_print("Exception: " + exn);
  do_check_true(exn.constructor.name == "TypeError");
});

add_task(function() {
  do_test_finished();
});
