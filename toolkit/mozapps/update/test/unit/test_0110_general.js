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
 * The Original Code is the Application Update Service.
 *
 * The Initial Developer of the Original Code is
 * Robert Strong <robert.bugzilla@gmail.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
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

/* General Complete MAR File Patch Apply Test */

function run_test() {
  // The directory the updates will be applied to is the current working
  // directory and not dist/bin.
  var testDir = do_get_file("mar_test", true);
  // The mar files were created with all files in a subdirectory named
  // mar_test... clear it out of the way if it exists and then create it.
  try {
    if (testDir.exists())
      testDir.remove(true);
  }
  catch (e) {
    dump("Unable to remove directory\npath: " + testDir.path +
         "\nException: " + e + "\n");
  }
  dump("Testing: successful removal of the directory used to apply the mar file\n");
  do_check_false(testDir.exists());
  testDir = do_get_file("mar_test/1/1_1/", true);
  testDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);

  // Create an empty test file to test the complete mar's ability to replace an
  // existing file.
  var testFile = do_get_file("mar_test/1/1_1/1_1_text1", true);
  testFile.create(AUS_Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);

  var binDir = getGREDir();

  // The updater binary file
  var updater = binDir.clone();
  updater.append("updater.app");
  if (!updater.exists()) {
    updater = binDir.clone();
    updater.append("updater.exe");
    if (!updater.exists()) {
      updater = binDir.clone();
      updater.append("updater");
      if (!updater.exists()) {
        do_throw("Unable to find updater binary!");
      }
    }
  }

  // Use a directory outside of dist/bin to lessen the garbage in dist/bin
  var updatesSubDir = do_get_file("0110_complete_mar", true);
  try {
    // Mac OS X intermittently fails when removing the dir where the updater
    // binary was launched.
    if (updatesSubDir.exists())
      updatesSubDir.remove(true);
  }
  catch (e) {
    dump("Unable to remove directory\npath: " + updatesSubDir.path +
         "\nException: " + e + "\n");
  }

  var mar = do_get_file("data/aus-0110_general.mar");
  mar.copyTo(updatesSubDir, "update.mar");

  // apply the complete mar and check the innards of the files
  var exitValue = runUpdate(updatesSubDir, updater);
  dump("Testing: updater binary process exitValue for success when applying " +
       "a complete mar\n");
  do_check_eq(exitValue, 0);

  dump("Testing: update.status should be set to STATE_SUCCEEDED\n");
  testFile = updatesSubDir.clone();
  testFile.append("update.status");
  do_check_eq(readFile(testFile).split("\n")[0], STATE_SUCCEEDED);

  dump("Testing: contents of files added by a complete mar\n");
  do_check_eq(getFileBytes(do_get_file("mar_test/1/1_1/1_1_text1", true)),
              "ToBeModified\n");
  do_check_eq(getFileBytes(do_get_file("mar_test/1/1_1/1_1_text2", true)),
              "ToBeDeleted\n");

  var refImage = do_get_file("data/aus-0110_general_ref_image.png");
  var srcImage = do_get_file("mar_test/1/1_1/1_1_image1.png", true);
  do_check_eq(getFileBytes(srcImage), getFileBytes(refImage));

  do_check_eq(getFileBytes(do_get_file("mar_test/2/2_1/2_1_text1", true)),
              "ToBeDeleted\n");

  try {
    // Mac OS X intermittently fails when removing the dir where the updater
    // binary was launched.
    if (updatesSubDir.exists())
      updatesSubDir.remove(true);
  }
  catch (e) {
    dump("Unable to remove directory\npath: " + updatesSubDir.path +
         "\nException: " + e + "\n");
  }

  cleanUp();
}

// Launches the updater binary to apply a mar file
function runUpdate(aUpdatesSubDir, aUpdater) {
  // Copy the updater binary to the update directory so the updater.ini is not
  // in the same directory as it is. This prevents ui from displaying and the
  // PostUpdate executable which is defined in the updater.ini from launching.
  aUpdater.copyTo(aUpdatesSubDir, aUpdater.leafName);
  var updateBin = aUpdatesSubDir.clone();
  updateBin.append(aUpdater.leafName);
  if (updateBin.leafName == "updater.app") {
    updateBin.append("Contents");
    updateBin.append("MacOS");
    updateBin.append("updater");
    if (!updateBin.exists())
      do_throw("Unable to find the updater executable!");
  }

  var updatesSubDirPath = aUpdatesSubDir.path;
  if (/ /.test(updatesSubDirPath))
    updatesSubDirPath = '"' + updatesSubDirPath + '"';

  var cwdPath = do_get_file("/", true).path;
  if (/ /.test(cwdPath))
    cwdPath = '"' + cwdPath + '"';

  var process = AUS_Cc["@mozilla.org/process/util;1"].
                createInstance(AUS_Ci.nsIProcess);
  process.init(updateBin);
  var args = [updatesSubDirPath, 0, cwdPath];
  process.run(true, args, args.length);
  return process.exitValue;
}

// Returns the binary contents of a file
function getFileBytes(aFile) {
  var fis = AUS_Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(AUS_Ci.nsIFileInputStream);
  fis.init(aFile, -1, -1, false);
  var bis = AUS_Cc["@mozilla.org/binaryinputstream;1"].
            createInstance(AUS_Ci.nsIBinaryInputStream);
  bis.setInputStream(fis);
  var data = bis.readBytes(bis.available());
  bis.close();
  fis.close();
  return data;
}
