/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var Ci = Components.interfaces;
var Cc = Components.classes;
const NS_ERROR_IN_PROGRESS = 2152398863;

const PR_RDONLY      = 0x01
const PR_WRONLY      = 0x02
const PR_RDWR        = 0x04
const PR_CREATE_FILE = 0x08
const PR_APPEND      = 0x10
const PR_TRUNCATE    = 0x20
const PR_SYNC        = 0x40
const PR_EXCL        = 0x80

const ZIP_EOCDR_HEADER_SIZE = 22;
const ZIP_FILE_HEADER_SIZE = 30;
const ZIP_CDS_HEADER_SIZE = 46;
const ZIP_METHOD_STORE = 0
const ZIP_METHOD_DEFLATE = 8
const ZIP_EXTENDED_TIMESTAMP_SIZE = 9;

const PR_USEC_PER_MSEC = 1000;
const PR_USEC_PER_SEC  = 1000000;
const PR_MSEC_PER_SEC  = 1000;

const DATA_DIR = "data/";

var ioSvc = Cc["@mozilla.org/network/io-service;1"]
             .getService(Ci.nsIIOService);

var ZipWriter = Components.Constructor("@mozilla.org/zipwriter;1",
                                       "nsIZipWriter");
var ZipReader = Components.Constructor("@mozilla.org/libjar/zip-reader;1",
                                       "nsIZipReader", "open");

var tmpDir = do_get_profile();
var tmpFile = tmpDir.clone();
tmpFile.append("zipwriter-test.zip");
if (tmpFile.exists())
  tmpFile.remove(true);

var zipW = new ZipWriter();

registerCleanupFunction(function() {
  try {
    zipW.close();
  } catch (e) {
    // Just ignore a failure here and attempt to delete the file anyway.
  }
  if (tmpFile.exists())
    tmpFile.remove(true);
});
