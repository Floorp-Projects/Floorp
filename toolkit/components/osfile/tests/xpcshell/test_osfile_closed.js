"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");

function run_test() {
  do_test_pending();
  run_next_test();
}

add_task(function test_closed() {
  OS.Shared.DEBUG = true;
  let currentDir = yield OS.File.getCurrentDirectory();
  do_print("Open a file, ensure that we can call stat()");
  let path = OS.Path.join(currentDir, "test_osfile_closed.js");
  let file = yield OS.File.open(path);
  yield file.stat();
  do_check_true(true);

  yield file.close();

  do_print("Ensure that we cannot stat() on closed file");
  let exn;
  try {
    yield file.stat();
  } catch (ex) {
    exn = ex;
  }
  do_print("Ensure that this raises the correct error");
  do_check_true(!!exn);
  do_check_true(exn instanceof OS.File.Error);
  do_check_true(exn.becauseClosed);

  do_print("Ensure that we cannot read() on closed file");
  exn = null;
  try {
    yield file.read();
  } catch (ex) {
    exn = ex;
  }
  do_print("Ensure that this raises the correct error");
  do_check_true(!!exn);
  do_check_true(exn instanceof OS.File.Error);
  do_check_true(exn.becauseClosed);

});

add_task(do_test_finished);
