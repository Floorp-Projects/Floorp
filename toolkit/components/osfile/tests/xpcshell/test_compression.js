/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");

function run_test() {
  do_test_pending();
  run_next_test();
}

add_task(function test_compress_lz4() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir, "compression.lz");
  let array = new Uint8Array(1024);
  for (let i = 0; i < array.byteLength; ++i) {
    array[i] = i;
  }

  do_print("Writing data with lz4 compression");
  let bytes = yield OS.File.writeAtomic(path, array, { compression: "lz4" });
  do_print("Compressed " + array.byteLength + " bytes into " + bytes);

  do_print("Reading back with lz4 decompression");
  let decompressed = yield OS.File.read(path, { compression: "lz4" });
  do_print("Decompressed into " + decompressed.byteLength + " bytes");
  do_check_eq(Array.prototype.join.call(array), Array.prototype.join.call(decompressed));
});

add_task(function test_uncompressed() {
  do_print("Writing data without compression");
  let path = OS.Path.join(OS.Constants.Path.tmpDir, "no_compression.tmp");
  let array = new Uint8Array(1024);
  for (let i = 0; i < array.byteLength; ++i) {
    array[i] = i;
  }
  let bytes = yield OS.File.writeAtomic(path, array); // No compression

  let exn;
  // Force decompression, reading should fail
  try {
    yield OS.File.read(path, { compression: "lz4" });
  } catch (ex) {
    exn = ex;
  }
  do_check_true(!!exn);
  do_check_true(exn.message.indexOf("Invalid header") != -1);
});

add_task(function() {
  do_test_finished();
});
