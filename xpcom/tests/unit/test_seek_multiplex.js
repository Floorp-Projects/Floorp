/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cr = Components.results;
var CC = Components.Constructor;
var Cc = Components.classes;

// The string we use as data.
const data = "0123456789";
// Number of streams in the multiplex stream.
const count = 10;

function test_multiplex_streams() {
  var MultiplexStream = CC("@mozilla.org/io/multiplex-input-stream;1",
                           "nsIMultiplexInputStream");
  do_check_eq(1, 1);

  var BinaryInputStream = Components.Constructor("@mozilla.org/binaryinputstream;1",
                                                 "nsIBinaryInputStream");
  var BinaryOutputStream = Components.Constructor("@mozilla.org/binaryoutputstream;1",
                                                  "nsIBinaryOutputStream",
                                                  "setOutputStream");
  var multiplex = new MultiplexStream();
  for (var i = 0; i < count; ++i) {
    let s = Cc["@mozilla.org/io/string-input-stream;1"]
              .createInstance(Ci.nsIStringInputStream);
    s.setData(data, data.length);

    multiplex.appendStream(s);
  }
  var seekable = multiplex.QueryInterface(Ci.nsISeekableStream);
  var sis = Cc["@mozilla.org/scriptableinputstream;1"]
              .createInstance(Ci.nsIScriptableInputStream);
  sis.init(seekable);
  // Read some data.
  var readData = sis.read(20);
  do_check_eq(readData, data + data);
  // -- Tests for NS_SEEK_SET
  // Seek to a non-zero, non-stream-boundary offset.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 2);
  do_check_eq(seekable.tell(), 2);
  do_check_eq(seekable.available(), 98);
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 9);
  do_check_eq(seekable.tell(), 9);
  do_check_eq(seekable.available(), 91);
  // Seek across stream boundary.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 35);
  do_check_eq(seekable.tell(), 35);
  do_check_eq(seekable.available(), 65);
  readData = sis.read(5);
  do_check_eq(readData, data.slice(5));
  do_check_eq(seekable.available(), 60);
  // Seek at stream boundaries.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 40);
  do_check_eq(seekable.tell(), 40);
  do_check_eq(seekable.available(), 60);
  readData = sis.read(10);
  do_check_eq(readData, data);
  do_check_eq(seekable.tell(), 50);
  do_check_eq(seekable.available(), 50);
  // Rewind and read across streams.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 39);
  do_check_eq(seekable.tell(), 39);
  do_check_eq(seekable.available(), 61);
  readData = sis.read(11);
  do_check_eq(readData, data.slice(9) + data);
  do_check_eq(seekable.tell(), 50);
  do_check_eq(seekable.available(), 50);
  // Rewind to the beginning.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
  do_check_eq(seekable.tell(), 0);
  do_check_eq(seekable.available(), 100);
  // Seek to some random location
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 50);
  // -- Tests for NS_SEEK_CUR
  // Positive seek.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_CUR, 15);
  do_check_eq(seekable.tell(), 65);
  do_check_eq(seekable.available(), 35);
  readData = sis.read(10);
  do_check_eq(readData, data.slice(5) + data.slice(0, 5));
  do_check_eq(seekable.tell(), 75);
  do_check_eq(seekable.available(), 25);
  // Negative seek.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_CUR, -15);
  do_check_eq(seekable.tell(), 60);
  do_check_eq(seekable.available(), 40);
  readData = sis.read(10);
  do_check_eq(readData, data);
  do_check_eq(seekable.tell(), 70);
  do_check_eq(seekable.available(), 30);

  // -- Tests for NS_SEEK_END
  // Normal read.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_END, -5);
  do_check_eq(seekable.tell(), data.length * count - 5);
  readData = sis.read(5);
  do_check_eq(readData, data.slice(5));
  do_check_eq(seekable.tell(), data.length * count);
  // Read across streams.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_END, -15);
  do_check_eq(seekable.tell(), data.length * count - 15);
  readData = sis.read(15);
  do_check_eq(readData, data.slice(5) + data);
  do_check_eq(seekable.tell(), data.length * count);

  // -- Try to do various edge cases
  // Forward seek from the end, should throw.
  var caught = false;
  try {
    seekable.seek(Ci.nsISeekableStream.NS_SEEK_END, 15);
  } catch(e) {
    caught = true;
  }
  do_check_eq(caught, true);
  do_check_eq(seekable.tell(), data.length * count);
  // Backward seek from the beginning, should be clamped.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
  do_check_eq(seekable.tell(), 0);
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_CUR, -15);
  do_check_eq(seekable.tell(), 0);
  // Seek too far: should be clamped.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
  do_check_eq(seekable.tell(), 0);
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_CUR, 3 * data.length * count);
  do_check_eq(seekable.tell(), 100);
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, data.length * count);
  do_check_eq(seekable.tell(), 100);
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_CUR, -2 * data.length * count);
  do_check_eq(seekable.tell(), 0);
}

function test_multiplex_bug797871() {

  var data = "1234567890123456789012345678901234567890";

  var MultiplexStream = CC("@mozilla.org/io/multiplex-input-stream;1",
                           "nsIMultiplexInputStream");
  do_check_eq(1, 1);

  var BinaryInputStream = Components.Constructor("@mozilla.org/binaryinputstream;1",
                                                 "nsIBinaryInputStream");
  var BinaryOutputStream = Components.Constructor("@mozilla.org/binaryoutputstream;1",
                                                  "nsIBinaryOutputStream",
                                                  "setOutputStream");
  var multiplex = new MultiplexStream();
  let s = Cc["@mozilla.org/io/string-input-stream;1"]
            .createInstance(Ci.nsIStringInputStream);
  s.setData(data, data.length);

  multiplex.appendStream(s);

  var seekable = multiplex.QueryInterface(Ci.nsISeekableStream);
  var sis = Cc["@mozilla.org/scriptableinputstream;1"]
              .createInstance(Ci.nsIScriptableInputStream);
  sis.init(seekable);

  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 8);
  do_check_eq(seekable.tell(), 8);
  readData = sis.read(2);
  do_check_eq(seekable.tell(), 10);

  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 20);
  do_check_eq(seekable.tell(), 20);
}

function run_test() {
  test_multiplex_streams();
  test_multiplex_bug797871();
}

