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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

const DATA = "ZIP WRITER TEST DATA";

var TESTS = [];

function build_tests() {
  var id = 0;

  // Minimum mode is 0400
  for (let u = 4; u <= 7; u++) {
    for (let g = 0; g <= 7; g++) {
      for (let o = 0; o <= 7; o++) {
        TESTS[id] = {
          name: "test" + u + g + o,
          permission: (u << 6) + (g << 3) + o
        };
        id++;
      }
    }
  }
}

function run_test() {
  build_tests();

  var foStream = Cc["@mozilla.org/network/file-output-stream;1"].
                 createInstance(Ci.nsIFileOutputStream);

  var tmp = tmpDir.clone();
  tmp.append("temp-permissions");
  tmp.createUnique(Ci.nsILocalFile.DIRECTORY_TYPE, 0755);

  var file = tmp.clone();
  file.append("tempfile");

  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  for (let i = 0; i < TESTS.length; i++) {
    // Open the file with the permissions to match how the zipreader extracts
    // This obeys the umask
    foStream.init(file, 0x02 | 0x08 | 0x20, TESTS[i].permission, 0);
    foStream.close();

    // umask may have altered the permissions so test against what they really were.
    // This reduces the coverage of the test but there isn't much we can do
    var perm = file.permissions & 0xfff;
    if (TESTS[i].permission != perm) {
      dump("File permissions for " + TESTS[i].name + " were " + perm.toString(8) + "\n");
      TESTS[i].permission = perm;
    }

    zipW.addEntryFile(TESTS[i].name, Ci.nsIZipWriter.COMPRESSION_NONE, file, false);
    file.permissions = 0600;
    file.remove(true);
  }
  zipW.close();

  zipR = new ZipReader(tmpFile);
  for (let i = 0; i < TESTS.length; i++) {
    zipR.extract(TESTS[i].name, file);
    dump("Testing file permissions for " + TESTS[i].name + "\n");
    do_check_eq(file.permissions & 0xfff, TESTS[i].permission);
    do_check_false(file.isDirectory());
    file.permissions = 0600;
    file.remove(true);
  }
  zipR.close();

  tmp.remove(true);
}
