/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0
 */

function BinaryComparer(file, callback) {
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  this.length = file.fileSize;
  this.fileStream = Cc["@mozilla.org/binaryinputstream;1"].
                    createInstance(Ci.nsIBinaryInputStream);
  this.fileStream.setInputStream(fstream);
  this.offset = 0;
  this.callback = callback;
}

BinaryComparer.prototype = {
  fileStream: null,
  offset: null,
  length: null,
  callback: null,

  onStartRequest: function(aRequest, aContext) {
  },

  onStopRequest: function(aRequest, aContext, aStatusCode) {
    this.fileStream.close();
    Assert.equal(aStatusCode, Components.results.NS_OK);
    Assert.equal(this.offset, this.length);
    this.callback();
  },

  onDataAvailable: function(aRequest, aContext, aInputStream, aOffset, aCount) {
    var stream = Cc["@mozilla.org/binaryinputstream;1"].
                 createInstance(Ci.nsIBinaryInputStream);
    stream.setInputStream(aInputStream);
    var source, actual;
    for (var i = 0; i < aCount; i++) {
      try {
        source = this.fileStream.read8();
      }
      catch (e) {
        do_throw("Unable to read from file at offset " + this.offset + " " + e);
      }
      try {
        actual = stream.read8();
      }
      catch (e) {
        do_throw("Unable to read from converted stream at offset " + this.offset + " " + e);
      }
      // The byte at offset 9 is the OS byte (see RFC 1952, section 2.3), which
      // can legitimately differ when the source is compressed on different
      // operating systems.  The actual .gz for this test was created on a Unix
      // system, but we want the test to work correctly everywhere.  So ignore
      // the byte at offset 9.
      if (this.offset == 9)
	;
      else if (source != actual)
        do_throw("Invalid value " + actual + " at offset " + this.offset + ", should have been " + source);
      this.offset++;
    }
  }
}

function comparer_callback()
{
  do_test_finished();
}

function run_test()
{
  var source = do_get_file(DATA_DIR + "test_bug717061.html");
  var comparer = new BinaryComparer(do_get_file(DATA_DIR + "test_bug717061.gz"),
                                    comparer_callback);
  
  // Prepare the stream converter
  var scs = Cc["@mozilla.org/streamConverters;1"].
            getService(Ci.nsIStreamConverterService);
  var converter = scs.asyncConvertData("uncompressed", "gzip", comparer, null);

  // Open the expected output file
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(source, -1, 0, 0);

  // Set up a pump to push data from the file to the stream converter
  var pump = Cc["@mozilla.org/network/input-stream-pump;1"].
             createInstance(Ci.nsIInputStreamPump);
  pump.init(fstream, 0, 0, true);
  pump.asyncRead(converter, null);
  do_test_pending();
}
