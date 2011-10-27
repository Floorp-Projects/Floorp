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

const NS_OS_TEMP_DIR = "TmpD";
const Ci = Components.interfaces;
const Cc = Components.classes;
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

var dirSvc = Cc["@mozilla.org/file/directory_service;1"]
              .getService(Ci.nsIProperties);
var tmpDir = dirSvc.get(NS_OS_TEMP_DIR, Ci.nsIFile);
var tmpFile = tmpDir.clone();
tmpFile.append("zipwriter-test.zip");
if (tmpFile.exists())
  tmpFile.remove(true);

var zipW = new ZipWriter();
