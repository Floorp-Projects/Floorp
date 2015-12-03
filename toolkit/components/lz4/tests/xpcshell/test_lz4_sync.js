/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Cu = Components.utils;
Cu.import("resource://gre/modules/lz4.js");
Cu.import("resource://gre/modules/osfile.jsm");

function run_test() {
  run_next_test();
}

function compare_arrays(a, b) {
  return Array.prototype.join.call(a) == Array.prototype.join.call(a);
}

add_task(function*() {
  let path = OS.Path.join("data", "compression.lz");
  let data = yield OS.File.read(path);
  let decompressed = Lz4.decompressFileContent(data);
  let text = (new TextDecoder()).decode(decompressed);
  do_check_eq(text, "Hello, lz4");
});

add_task(function*() {
  for (let length of [0, 1, 1024]) {
    let array = new Uint8Array(length);
    for (let i = 0; i < length; ++i) {
      array[i] = i % 256;
    }

    let compressed = Lz4.compressFileContent(array);
    do_print("Compressed " + array.byteLength + " bytes into " +
             compressed.byteLength);

    let decompressed = Lz4.decompressFileContent(compressed);
    do_print("Decompressed " + compressed.byteLength + " bytes into " +
             decompressed.byteLength);

    do_check_true(compare_arrays(array, decompressed));
  }
});
