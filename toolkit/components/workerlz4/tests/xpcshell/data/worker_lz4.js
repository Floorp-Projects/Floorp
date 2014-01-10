importScripts("resource://gre/modules/workers/require.js");
importScripts("resource://gre/modules/osfile.jsm");


function do_print(x) {
  //self.postMessage({kind: "do_print", args: [x]});
  dump("TEST-INFO: " + x + "\n");
}

function do_check_true(x) {
  self.postMessage({kind: "do_check_true", args: [!!x]});
  if (x) {
    dump("TEST-PASS: " + x + "\n");
  } else {
    throw new Error("do_check_true failed");
  }
}

function do_check_eq(a, b) {
  let result = a == b;
  self.postMessage({kind: "do_check_true", args: [result]});
  if (!result) {
    throw new Error("do_check_eq failed " + a + " != " + b);
  }
}

function do_test_complete() {
  self.postMessage({kind: "do_test_complete", args:[]});
}

self.onmessage = function() {
  try {
    run_test();
  } catch (ex) {
    let {message, moduleStack, moduleName, lineNumber} = ex;
    let error = new Error(message, moduleName, lineNumber);
    error.stack = moduleStack;
    dump("Uncaught error: " + error + "\n");
    dump("Full stack: " + moduleStack + "\n");
    throw error;
  }
};

let Lz4;
let Internals;
function test_import() {
  Lz4 = require("resource://gre/modules/workers/lz4.js");
  Internals = require("resource://gre/modules/workers/lz4_internal.js");
}

function test_bound() {
  for (let k of ["compress", "decompress", "maxCompressedSize"]) {
    try {
      do_print("Checking the existence of " + k + "\n");
      do_check_true(!!Internals[k]);
      do_print(k + " exists");
    } catch (ex) {
      // Ignore errors
      do_print(k + " doesn't exist!");
    }
  }
}

function test_reference_file() {
  do_print("Decompress reference file");
  let path = OS.Path.join("data", "compression.lz");
  let data = OS.File.read(path);
  let decompressed = Lz4.decompressFileContent(data);
  let text = (new TextDecoder()).decode(decompressed);
  do_check_eq(text, "Hello, lz4");
}

function compare_arrays(a, b) {
  return Array.prototype.join.call(a) == Array.prototype.join.call(a);
}

function run_rawcompression(name, array) {
  do_print("Raw compression test " + name);
  let length = array.byteLength;
  let compressedArray = new Uint8Array(Internals.maxCompressedSize(length));
  let compressedBytes = Internals.compress(array, length, compressedArray);
  compressedArray = new Uint8Array(compressedArray.buffer, 0, compressedBytes);
  do_print("Raw compressed: " + length + " into " + compressedBytes);

  let decompressedArray = new Uint8Array(length);
  let decompressedBytes = new ctypes.size_t();
  let success = Internals.decompress(compressedArray, compressedBytes,
                                     decompressedArray, length,
                                     decompressedBytes.address());
  do_print("Raw decompression success? " + success);
  do_print("Raw decompression size: " + decompressedBytes.value);
  do_check_true(compare_arrays(array, decompressedArray));
}

function run_filecompression(name, array) {
  do_print("File compression test " + name);
  let compressed = Lz4.compressFileContent(array);
  do_print("Compressed " + array.byteLength + " bytes into " + compressed.byteLength);

  let decompressed = Lz4.decompressFileContent(compressed);
  do_print("Decompressed " + compressed.byteLength + " bytes into " + decompressed.byteLength);
  do_check_true(compare_arrays(array, decompressed));
}

function run_faileddecompression(name, array) {
  do_print("invalid decompression test " + name);

  // Ensure that raw decompression doesn't segfault
  let length = 1 << 14;
  let decompressedArray = new Uint8Array(length);
  let decompressedBytes = new ctypes.size_t();
  Internals.decompress(array, array.byteLength,
    decompressedArray, length,
    decompressedBytes.address());

  // File decompression should fail with an acceptable exception
  let exn = null;
  try {
    Lz4.decompressFileContent(array);
  } catch (ex) {
    exn = ex;
  }
  do_check_true(exn);
  if (array.byteLength < 10) {
    do_check_true(exn.becauseLZNoHeader);
  } else {
    do_check_true(exn.becauseLZWrongMagicNumber);
  }
}

function run_test() {
  test_import();
  test_bound();
  test_reference_file();
  for (let length of [0, 1, 1024]) {
    let array = new Uint8Array(length);
    for (let i = 0; i < length; ++i) {
      array[i] = i % 256;
    }
    let name = length + " bytes";
    run_rawcompression(name, array);
    run_filecompression(name, array);
    run_faileddecompression(name, array);
  }
  do_test_complete();
}
