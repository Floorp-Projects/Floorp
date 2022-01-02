"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

function run_test() {
  do_test_pending();
  run_next_test();
}

/**
 * Test to ensure that {bytes:} in options to |write| is correctly
 * preserved.
 */
add_task(async function test_bytes() {
  let path = OS.Path.join(
    OS.Constants.Path.tmpDir,
    "test_osfile_async_bytes.tmp"
  );
  let file = await OS.File.open(path, { trunc: true, read: true, write: true });
  try {
    try {
      // 1. Test write, by supplying {bytes:} options smaller than the actual
      // buffer.
      await file.write(new Uint8Array(2048), { bytes: 1024 });
      Assert.equal((await file.stat()).size, 1024);

      // 2. Test that passing nullish values for |options| still works.
      await file.setPosition(0, OS.File.POS_END);
      await file.write(new Uint8Array(1024), null);
      await file.write(new Uint8Array(1024), undefined);
      Assert.equal((await file.stat()).size, 3072);
    } finally {
      await file.close();
    }
  } finally {
    await OS.File.remove(path);
  }
});

add_task(do_test_finished);
