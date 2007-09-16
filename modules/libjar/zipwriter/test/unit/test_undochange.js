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
  "test.txt",
  "test.png"
];

function run_test()
{
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  for (var i = 0; i < TESTS.length; i++) {
    var source = do_get_file(DATA_DIR + TESTS[i]);
    zipW.addEntryFile(TESTS[i], Ci.nsIZipWriter.COMPRESSION_NONE, source,
                      false);
  }

  try {
    var source = do_get_file(DATA_DIR + TESTS[0]);
    zipW.addEntryFile(TESTS[0], Ci.nsIZipWriter.COMPRESSION_NONE, source,
                      false);
    do_throw("Should not be able to add the same file twice");
  }
  catch (e) {
    do_check_eq(e.result, Components.results.NS_ERROR_FILE_ALREADY_EXISTS);
  }

  // Remove all the tests and see if we are left with an empty zip
  for (var i = 0; i < TESTS.length; i++) {
    zipW.removeEntry(TESTS[i], false);
  }

  zipW.close();

  // Empty zip file should just be the end of central directory marker
  do_check_eq(tmpFile.fileSize, ZIP_EOCDR_HEADER_SIZE);
}
