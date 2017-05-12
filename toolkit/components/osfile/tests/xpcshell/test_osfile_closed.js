"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");

function run_test() {
  do_test_pending();
  run_next_test();
}

add_task(async function test_closed() {
  OS.Shared.DEBUG = true;
  let currentDir = await OS.File.getCurrentDirectory();
  do_print("Open a file, ensure that we can call stat()");
  let path = OS.Path.join(currentDir, "test_osfile_closed.js");
  let file = await OS.File.open(path);
  await file.stat();
  do_check_true(true);

  await file.close();

  do_print("Ensure that we cannot stat() on closed file");
  let exn;
  try {
    await file.stat();
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
    await file.read();
  } catch (ex) {
    exn = ex;
  }
  do_print("Ensure that this raises the correct error");
  do_check_true(!!exn);
  do_check_true(exn instanceof OS.File.Error);
  do_check_true(exn.becauseClosed);

});

add_task(do_test_finished);
