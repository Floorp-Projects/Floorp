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
  Assert.equal(is.readBoolean(), true);
  os.write8(4);
  Assert.equal(is.read8(), 4);
  os.write16(4);
  Assert.equal(is.read16(), 4);
  os.write16(1024);
  Assert.equal(is.read16(), 1024);
  os.write32(7);
  Assert.equal(is.read32(), 7);
  os.write32(LargeNum);
  Assert.equal(is.read32(), LargeNum);
  os.write64(LargeNum);
  Assert.equal(is.read64(), LargeNum);
  os.write64(1024);
  Assert.equal(is.read64(), 1024);
  os.write64(HugeNum);
  Assert.equal(is.read64(), HugeNum);
  os.writeFloat(2.5);
  Assert.equal(is.readFloat(), 2.5);
//  os.writeDouble(Math.SQRT2);
//  do_check_eq(is.readDouble(), Math.SQRT2);
  os.writeStringZ("Mozilla");
  Assert.equal(is.readCString(), "Mozilla");
  os.writeWStringZ("Gecko");
  Assert.equal(is.readString(), "Gecko");
  os.writeBytes(HelloStr, HelloStr.length);
  Assert.equal(is.available(), HelloStr.length);
  msg = is.readBytes(HelloStr.length);
  Assert.equal(msg, HelloStr);
  msg = null;
  countObj.value = -1;
  os.writeByteArray(HelloArray, HelloArray.length);
  Assert.equal(is.available(), HelloStr.length);
  msg = is.readByteArray(HelloStr.length);
  Assert.equal(typeof msg, typeof HelloArray);
  Assert.equal(msg.toSource(), HelloArray.toSource());
  Assert.equal(is.available(), 0);
  os.writeByteArray(HelloArray, HelloArray.length);
  Assert.equal(is.readArrayBuffer(buffer.byteLength, buffer), HelloArray.length);
  Assert.equal([...(new Uint8Array(buffer))].toSource(), HelloArray.toSource());
  Assert.equal(is.available(), 0);

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
  Assert.notEqual(is.available(), 0);

  // Test reading in one big chunk.
  Assert.equal(is.readBoolean(), true);
  Assert.equal(is.read8(), 4);
  Assert.equal(is.read16(), 4);
  Assert.equal(is.read16(), 1024);
  Assert.equal(is.read32(), 7);
  Assert.equal(is.read32(), LargeNum);
  Assert.equal(is.read64(), LargeNum);
  Assert.equal(is.read64(), 1024);
  Assert.equal(is.read64(), HugeNum);
  Assert.equal(is.readFloat(), 2.5);
//  do_check_eq(is.readDouble(), Math.SQRT2);
  Assert.equal(is.readCString(), "Mozilla");
  Assert.equal(is.readString(), "Gecko");
  // Remember, we wrote HelloStr twice - once as a string, and then as an array.
  Assert.equal(is.available(), HelloStr.length * 2);
  msg = is.readBytes(HelloStr.length);
  Assert.equal(msg, HelloStr);
  msg = null;
  countObj.value = -1;
  Assert.equal(is.available(), HelloStr.length);
  msg = is.readByteArray(HelloStr.length);
  Assert.equal(typeof msg, typeof HelloArray);
  Assert.equal(msg.toSource(), HelloArray.toSource());
  Assert.equal(is.available(), 0);

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
