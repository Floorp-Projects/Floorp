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
 *     Kris Maglione <maglione.k@gmail.com>
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

// Values taken from using zipinfo to list the test.zip contents
var TESTS = [
  {
    name: "test.txt",
    size: 232,
    crc: 0x0373ac26
  },
  {
    name: "test.png",
    size: 3402,
    crc: 0x504a5c30
  }
];

var size = 0;

var observer = {
  onStartRequest: function(request, context)
  {
  },

  onStopRequest: function(request, context, status)
  {
    do_check_eq(status, Components.results.NS_OK);

    zipW.close();
    size += ZIP_EOCDR_HEADER_SIZE;

    do_check_eq(size, tmpFile.fileSize);

    // Test the stored data with the zipreader
    var zipR = new ZipReader(tmpFile);

    for (var i = 0; i < TESTS.length; i++) {
      var source = do_get_file(DATA_DIR + TESTS[i].name);
      for (let method in methods) {
        var entryName = method + "/" + TESTS[i].name;
        do_check_true(zipR.hasEntry(entryName));

        var entry = zipR.getEntry(entryName);
        do_check_eq(entry.realSize, TESTS[i].size);
        do_check_eq(entry.size, TESTS[i].size);
        do_check_eq(entry.CRC32, TESTS[i].crc);
        do_check_eq(Math.floor(entry.lastModifiedTime / PR_USEC_PER_SEC),
                    Math.floor(source.lastModifiedTime / PR_MSEC_PER_SEC));

        zipR.test(entryName);
      }
    }

    zipR.close();
    do_test_finished();
  }
};

var methods = {
  file: function method_file(entry, source)
  {
    zipW.addEntryFile(entry, Ci.nsIZipWriter.COMPRESSION_NONE, source,
                      true);
  },
  channel: function method_channel(entry, source)
  {
    zipW.addEntryChannel(entry, source.lastModifiedTime * PR_MSEC_PER_SEC,
                         Ci.nsIZipWriter.COMPRESSION_NONE,
                         ioSvc.newChannelFromURI(ioSvc.newFileURI(source)), true);
  },
  stream: function method_stream(entry, source)
  {
    zipW.addEntryStream(entry, source.lastModifiedTime * PR_MSEC_PER_SEC,
                        Ci.nsIZipWriter.COMPRESSION_NONE,
                        ioSvc.newChannelFromURI(ioSvc.newFileURI(source)).open(), true);
  }
}

function run_test()
{
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  for (var i = 0; i < TESTS.length; i++) {
    var source = do_get_file(DATA_DIR+TESTS[i].name);
    for (let method in methods) {
      var entry = method + "/" + TESTS[i].name;
      methods[method](entry, source);
      size += ZIP_FILE_HEADER_SIZE + ZIP_CDS_HEADER_SIZE +
              (ZIP_EXTENDED_TIMESTAMP_SIZE * 2) +
              (entry.length*2) + TESTS[i].size;
    }
  }
  do_test_pending();
  zipW.processQueue(observer, null);
  do_check_true(zipW.inQueue);
}
