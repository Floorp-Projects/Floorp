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

// Values taken from using zipinfo to list the test.zip contents
var TESTS = [
  {
    name: "test.txt",
    size: 232,
    crc: 0x0373ac26,
    time: new Date(2007, 4, 1, 21, 44, 56)
  },
  {
    name: "test.png",
    size: 3402,
    crc: 0x504a5c30,
    time: new Date(2007, 4, 1, 21, 49, 40)
  }
];
var BADENTRY = "unknown.txt";

function run_test()
{
  // Copy our test zip to the tmp dir so we can modify it
  var testzip = do_get_file(DATA_DIR + "test.zip");
  testzip.copyTo(tmpDir, tmpFile.leafName);

  do_check_true(tmpFile.exists());

  zipW.open(tmpFile, PR_RDWR);

  for (var i = 0; i < TESTS.length; i++) {
    do_check_true(zipW.hasEntry(TESTS[i].name));
    var entry = zipW.getEntry(TESTS[i].name);
    do_check_true(entry != null);

    do_check_eq(entry.realSize, TESTS[i].size);
    do_check_eq(entry.CRC32, TESTS[i].crc);
    var diff = Math.abs(TESTS[i].time -
               (entry.lastModifiedTime / PR_USEC_PER_MSEC));
    if (diff > TIME_RESOLUTION)
      do_throw(diff);
  }

  try {
    zipW.removeEntry(BADENTRY, false);
    do_throw("shouldn't be able to remove an entry that doesn't exist");
  }
  catch (e) {
    do_check_eq(e.result, Components.results.NS_ERROR_FILE_NOT_FOUND);
  }

  for (var i = 0; i < TESTS.length; i++) {
    zipW.removeEntry(TESTS[i].name, false);
  }

  zipW.close();

  // Certain platforms cache the file size so get a fresh file to check.
  tmpFile = tmpFile.clone();

  // Empty zip file should just be the end of central directory marker
  do_check_eq(tmpFile.fileSize, ZIP_EOCDR_HEADER_SIZE);
}
