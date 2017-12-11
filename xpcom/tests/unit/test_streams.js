/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Pipe = CC("@mozilla.org/pipe;1",
              "nsIPipe",
              "init");
var BinaryOutput = CC("@mozilla.org/binaryoutputstream;1",
                      "nsIBinaryOutputStream",
                      "setOutputStream");
var BinaryInput = CC("@mozilla.org/binaryinputstream;1",
                     "nsIBinaryInputStream",
                     "setInputStream");

/**
 * Binary stream tests.
 */
function test_binary_streams() {
  var p, is, os;

  p = new Pipe(false, false, 1024, 1, null);
  is = new BinaryInput(p.inputStream);
  os = new BinaryOutput(p.outputStream);

  const LargeNum = Math.pow(2, 18) + Math.pow(2, 12) + 1;
  const HugeNum = Math.pow(2, 62);
  const HelloStr = "Hello World";
  const HelloArray = Array.map(HelloStr, function(c) { return c.charCodeAt(0); });
  var countObj = {};
  var msg = {};
  var buffer = new ArrayBuffer(HelloArray.length);

  // Test reading immediately after writing.
  os.writeBoolean(true);
  do_check_eq(is.readBoolean(), true);
  os.write8(4);
  do_check_eq(is.read8(), 4);
  os.write16(4);
  do_check_eq(is.read16(), 4);
  os.write16(1024);
  do_check_eq(is.read16(), 1024);
  os.write32(7);
  do_check_eq(is.read32(), 7);
  os.write32(LargeNum);
  do_check_eq(is.read32(), LargeNum);
  os.write64(LargeNum);
  do_check_eq(is.read64(), LargeNum);
  os.write64(1024);
  do_check_eq(is.read64(), 1024);
  os.write64(HugeNum);
  do_check_eq(is.read64(), HugeNum);
  os.writeFloat(2.5);
  do_check_eq(is.readFloat(), 2.5);
//  os.writeDouble(Math.SQRT2);
//  do_check_eq(is.readDouble(), Math.SQRT2);
  os.writeStringZ("Mozilla");
  do_check_eq(is.readCString(), "Mozilla");
  os.writeWStringZ("Gecko");
  do_check_eq(is.readString(), "Gecko");
  os.writeBytes(HelloStr, HelloStr.length);
  do_check_eq(is.available(), HelloStr.length);
  msg = is.readBytes(HelloStr.length);
  do_check_eq(msg, HelloStr);
  msg = null;
  countObj.value = -1;
  os.writeByteArray(HelloArray, HelloArray.length);
  do_check_eq(is.available(), HelloStr.length);
  msg = is.readByteArray(HelloStr.length);
  do_check_eq(typeof msg, typeof HelloArray);
  do_check_eq(msg.toSource(), HelloArray.toSource());
  do_check_eq(is.available(), 0);
  os.writeByteArray(HelloArray, HelloArray.length);
  do_check_eq(is.readArrayBuffer(buffer.byteLength, buffer), HelloArray.length);
  do_check_eq([...(new Uint8Array(buffer))].toSource(), HelloArray.toSource());
  do_check_eq(is.available(), 0);

  // Test writing in one big chunk.
  os.writeBoolean(true);
  os.write8(4);
  os.write16(4);
  os.write16(1024);
  os.write32(7);
  os.write32(LargeNum);
  os.write64(LargeNum);
  os.write64(1024);
  os.write64(HugeNum);
  os.writeFloat(2.5);
//  os.writeDouble(Math.SQRT2);
  os.writeStringZ("Mozilla");
  os.writeWStringZ("Gecko");
  os.writeBytes(HelloStr, HelloStr.length);
  os.writeByteArray(HelloArray, HelloArray.length);
  // Available should not be zero after a long write like this.
  do_check_neq(is.available(), 0);

  // Test reading in one big chunk.
  do_check_eq(is.readBoolean(), true);
  do_check_eq(is.read8(), 4);
  do_check_eq(is.read16(), 4);
  do_check_eq(is.read16(), 1024);
  do_check_eq(is.read32(), 7);
  do_check_eq(is.read32(), LargeNum);
  do_check_eq(is.read64(), LargeNum);
  do_check_eq(is.read64(), 1024);
  do_check_eq(is.read64(), HugeNum);
  do_check_eq(is.readFloat(), 2.5);
//  do_check_eq(is.readDouble(), Math.SQRT2);
  do_check_eq(is.readCString(), "Mozilla");
  do_check_eq(is.readString(), "Gecko");
  // Remember, we wrote HelloStr twice - once as a string, and then as an array.
  do_check_eq(is.available(), HelloStr.length * 2);
  msg = is.readBytes(HelloStr.length);
  do_check_eq(msg, HelloStr);
  msg = null;
  countObj.value = -1;
  do_check_eq(is.available(), HelloStr.length);
  msg = is.readByteArray(HelloStr.length);
  do_check_eq(typeof msg, typeof HelloArray);
  do_check_eq(msg.toSource(), HelloArray.toSource());
  do_check_eq(is.available(), 0);

  // Test for invalid actions.
  os.close();
  is.close();

  try {
    os.writeBoolean(false);
    do_throw("Not reached!");
  } catch (e) {
    if (!(e instanceof Ci.nsIException &&
          e.result == Cr.NS_BASE_STREAM_CLOSED)) {
      throw e;
    }
    // do nothing
  }

  try {
    is.available();
    do_throw("Not reached!");
  } catch (e) {
    if (!(e instanceof Ci.nsIException &&
          e.result == Cr.NS_BASE_STREAM_CLOSED)) {
      throw e;
    }
    // do nothing
  }

  try {
    is.readBoolean();
    do_throw("Not reached!");
  } catch (e) {
    if (!(e instanceof Ci.nsIException &&
          e.result == Cr.NS_ERROR_FAILURE)) {
      throw e;
    }
    // do nothing
  }
}

function run_test() {
  test_binary_streams();
}
