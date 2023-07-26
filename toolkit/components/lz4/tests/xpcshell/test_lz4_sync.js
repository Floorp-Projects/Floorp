/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { Lz4 } = ChromeUtils.import("resource://gre/modules/lz4.js");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

function compare_arrays(a, b) {
  return Array.prototype.join.call(a) == Array.prototype.join.call(b);
}

add_task(async function() {
  let path = OS.Path.join("data", "compression.lz");
  let data = await OS.File.read(path);
  let decompressed = Lz4.decompressFileContent(data);
  let text = new TextDecoder().decode(decompressed);
  Assert.equal(text, "Hello, lz4");
});

add_task(async function() {
  for (let length of [0, 1, 1024]) {
    let array = new Uint8Array(length);
    for (let i = 0; i < length; ++i) {
      array[i] = i % 256;
    }

    let compressed = Lz4.compressFileContent(array);
    info(
      "Compressed " + array.byteLength + " bytes into " + compressed.byteLength
    );

    let decompressed = Lz4.decompressFileContent(compressed);
    info(
      "Decompressed " +
        compressed.byteLength +
        " bytes into " +
        decompressed.byteLength
    );

    Assert.ok(compare_arrays(array, decompressed));
  }
});
