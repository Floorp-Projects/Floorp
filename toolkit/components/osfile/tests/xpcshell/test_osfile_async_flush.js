"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

function run_test() {
  do_test_pending();
  run_next_test();
}

/**
 * Test to ensure that |File.prototype.flush| is available in the async API.
 */

add_task(async function test_flush() {
  let path = OS.Path.join(
    OS.Constants.Path.tmpDir,
    "test_osfile_async_flush.tmp"
  );
  let file = await OS.File.open(path, { trunc: true, write: true });
  try {
    try {
      await file.flush();
    } finally {
      await file.close();
    }
  } finally {
    await OS.File.remove(path);
  }
});

add_task(do_test_finished);
