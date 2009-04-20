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

/* General Partial MAR File Patch Apply Tests */

function run_test() {
  // The directory the updates will be applied to is the current working
  // directory and not dist/bin.
  var testDir = do_get_cwd();
  // The mar files were created with all files in a subdirectory named
  // mar_test... clear it out of the way if it exists and then create it.
  testDir.append("mar_test");
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
  testDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, 0755);

  // Create the files to test the partial mar's ability to modify and delete
  // files.
  var testFile = testDir.clone();
  testFile.append("text1");
  writeFile(testFile, "ToBeModified\n");

  testFile = testDir.clone();
  testFile.append("text2");
  writeFile(testFile, "ToBeDeleted\n");

  testFile = do_get_file("data/aus-0110_general_ref_image1.png");
  testFile.copyTo(testDir, "image1.png");

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
  var updatesSubDir = do_get_cwd();
  updatesSubDir.append("0111_partial_mar");

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

  var mar = do_get_file("data/aus-0110_general-2.mar");
  mar.copyTo(updatesSubDir, "update.mar");

  // apply the partial mar and check the innards of the files
  exitValue = runUpdate(updatesSubDir, updater);
  dump("Testing: updater binary process exitValue for success when applying " +
       "a partial mar\n");
  do_check_eq(exitValue, 0);

  dump("Testing: removal of a file and contents of added / modified files by " +
       "a partial mar\n");
  do_check_eq(getFileBytes(getTestFile(testDir, "text1")), "Modified\n");
  do_check_false(getTestFile(testDir, "text2").exists()); // file removed
  do_check_eq(getFileBytes(getTestFile(testDir, "text3")), "Added\n");

  refImage = do_get_file("data/aus-0110_general_ref_image2.png");
  srcImage = getTestFile(testDir, "image1.png");
  do_check_eq(getFileBytes(srcImage), getFileBytes(refImage));

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

  removeUpdateDirsAndFiles();
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

  var process = AUS_Cc["@mozilla.org/process/util;1"]
                  .createInstance(AUS_Ci.nsIProcess);
  process.init(updateBin);
  var args = [updatesSubDirPath];
  process.run(true, args, args.length);
  return process.exitValue;
}

// Gets a file in the mar_test subdirectory of the current working directory
// which is where the mar will be applied.
function getTestFile(aDir, aLeafName) {
  var file = aDir.clone();
  file.append(aLeafName);
  if (!(file instanceof AUS_Ci.nsILocalFile))
    do_throw("File must be a nsILocalFile for this test! File: " + aLeafName);

  return file;
}

// Returns the binary contents of a file
function getFileBytes(aFile) {
  var fis = AUS_Cc["@mozilla.org/network/file-input-stream;1"]
              .createInstance(AUS_Ci.nsIFileInputStream);
  fis.init(aFile, -1, -1, false);
  var bis = AUS_Cc["@mozilla.org/binaryinputstream;1"]
              .createInstance(AUS_Ci.nsIBinaryInputStream);
  bis.setInputStream(fis);
  var data = bis.readBytes(bis.available());
  bis.close();
  fis.close();
  return data;
}
