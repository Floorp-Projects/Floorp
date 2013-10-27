"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");

function run_test() {
  do_test_pending();
  run_next_test();
}

/**
 * Test to ensure that |File.prototype.flush| is available in the async API.
 */

add_task(function test_flush() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                          "test_osfile_async_flush.tmp");
  let file = yield OS.File.open(path, {trunc: true, write: true});
  try {
    try {
      yield file.flush();
    } finally {
      yield file.close();
    }
  } finally {
    yield OS.File.remove(path);
  }
});

add_task(do_test_finished);
