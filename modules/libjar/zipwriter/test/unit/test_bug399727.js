/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Zip Writer Component.
 *
 * The Initial Developer of the Original Code is
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
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
    do_check_eq(aStatusCode, Components.results.NS_OK);
    do_check_eq(this.offset, this.length);
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
      if (source != actual)
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
  var source = do_get_file(DATA_DIR + "test_bug399727.html");
  var comparer = new BinaryComparer(do_get_file(DATA_DIR + "test_bug399727.zlib"),
                                    comparer_callback);
  
  // Prepare the stream converter
  var scs = Cc["@mozilla.org/streamConverters;1"].
            getService(Ci.nsIStreamConverterService);
  var converter = scs.asyncConvertData("uncompressed", "deflate", comparer, null);

  // Open the expected output file
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(source, -1, 0, 0);

  // Set up a pump to push data from the file to the stream converter
  var pump = Cc["@mozilla.org/network/input-stream-pump;1"].
             createInstance(Ci.nsIInputStreamPump);
  pump.init(fstream, -1, -1, 0, 0, true);
  pump.asyncRead(converter, null);
  do_test_pending();
}
