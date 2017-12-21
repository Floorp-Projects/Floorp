/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The string we use as data.
const data = "0123456789";
// Number of streams in the multiplex stream.
const count = 10;

function test_multiplex_streams() {
  var MultiplexStream = CC("@mozilla.org/io/multiplex-input-stream;1",
                           "nsIMultiplexInputStream");
  Assert.equal(1, 1);

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
  Assert.equal(readData, data + data);
  // -- Tests for NS_SEEK_SET
  // Seek to a non-zero, non-stream-boundary offset.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 2);
  Assert.equal(seekable.tell(), 2);
  Assert.equal(seekable.available(), 98);
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 9);
  Assert.equal(seekable.tell(), 9);
  Assert.equal(seekable.available(), 91);
  // Seek across stream boundary.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 35);
  Assert.equal(seekable.tell(), 35);
  Assert.equal(seekable.available(), 65);
  readData = sis.read(5);
  Assert.equal(readData, data.slice(5));
  Assert.equal(seekable.available(), 60);
  // Seek at stream boundaries.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 40);
  Assert.equal(seekable.tell(), 40);
  Assert.equal(seekable.available(), 60);
  readData = sis.read(10);
  Assert.equal(readData, data);
  Assert.equal(seekable.tell(), 50);
  Assert.equal(seekable.available(), 50);
  // Rewind and read across streams.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 39);
  Assert.equal(seekable.tell(), 39);
  Assert.equal(seekable.available(), 61);
  readData = sis.read(11);
  Assert.equal(readData, data.slice(9) + data);
  Assert.equal(seekable.tell(), 50);
  Assert.equal(seekable.available(), 50);
  // Rewind to the beginning.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
  Assert.equal(seekable.tell(), 0);
  Assert.equal(seekable.available(), 100);
  // Seek to some random location
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 50);
  // -- Tests for NS_SEEK_CUR
  // Positive seek.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_CUR, 15);
  Assert.equal(seekable.tell(), 65);
  Assert.equal(seekable.available(), 35);
  readData = sis.read(10);
  Assert.equal(readData, data.slice(5) + data.slice(0, 5));
  Assert.equal(seekable.tell(), 75);
  Assert.equal(seekable.available(), 25);
  // Negative seek.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_CUR, -15);
  Assert.equal(seekable.tell(), 60);
  Assert.equal(seekable.available(), 40);
  readData = sis.read(10);
  Assert.equal(readData, data);
  Assert.equal(seekable.tell(), 70);
  Assert.equal(seekable.available(), 30);

  // -- Tests for NS_SEEK_END
  // Normal read.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_END, -5);
  Assert.equal(seekable.tell(), data.length * count - 5);
  readData = sis.read(5);
  Assert.equal(readData, data.slice(5));
  Assert.equal(seekable.tell(), data.length * count);
  // Read across streams.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_END, -15);
  Assert.equal(seekable.tell(), data.length * count - 15);
  readData = sis.read(15);
  Assert.equal(readData, data.slice(5) + data);
  Assert.equal(seekable.tell(), data.length * count);

  // -- Try to do various edge cases
  // Forward seek from the end, should throw.
  var caught = false;
  try {
    seekable.seek(Ci.nsISeekableStream.NS_SEEK_END, 15);
  } catch (e) {
    caught = true;
  }
  Assert.equal(caught, true);
  Assert.equal(seekable.tell(), data.length * count);
  // Backward seek from the beginning, should be clamped.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
  Assert.equal(seekable.tell(), 0);
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_CUR, -15);
  Assert.equal(seekable.tell(), 0);
  // Seek too far: should be clamped.
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
  Assert.equal(seekable.tell(), 0);
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_CUR, 3 * data.length * count);
  Assert.equal(seekable.tell(), 100);
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, data.length * count);
  Assert.equal(seekable.tell(), 100);
  seekable.seek(Ci.nsISeekableStream.NS_SEEK_CUR, -2 * data.length * count);
  Assert.equal(seekable.tell(), 0);
}

function test_multiplex_bug797871() {

  var data2 = "1234567890123456789012345678901234567890";

  var MultiplexStream = CC("@mozilla.org/io/multiplex-input-stream;1",
                           "nsIMultiplexInputStream");
  Assert.equal(1, 1);

  var multiplex = new MultiplexStream();
  let s = Cc["@mozilla.org/io/string-input-stream;1"]
            .createInstance(Ci.nsIStringInputStream);
  s.setData(data2, data2.length);

  multiplex.appendStream(s);

  var seekable = multiplex.QueryInterface(Ci.nsISeekableStream);
  var sis = Cc["@mozilla.org/scriptableinputstream;1"]
              .createInstance(Ci.nsIScriptableInputStream);
  sis.init(seekable);

  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 8);
  Assert.equal(seekable.tell(), 8);
  sis.read(2);
  Assert.equal(seekable.tell(), 10);

  seekable.seek(Ci.nsISeekableStream.NS_SEEK_SET, 20);
  Assert.equal(seekable.tell(), 20);
}

function run_test() {
  test_multiplex_streams();
  test_multiplex_bug797871();
}
