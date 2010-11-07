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

const APPLY_TO_DIR = "applyToDir_0110";
const UPDATES_DIR  = "0110_mar";
const AFTER_APPLY_DIR = "afterApplyDir";
const UPDATER_BIN_FILE = "updater" + BIN_SUFFIX;
const AFTER_APPLY_BIN_FILE = "TestAUSHelper" + BIN_SUFFIX;
const RELAUNCH_BIN_FILE = "relaunch_app" + BIN_SUFFIX;
const RELAUNCH_ARGS = ["Test Arg 1", "Test Arg 2", "Test Arg 3"];
// All we care about is that the last modified time has changed so that Mac OS
// X Launch Services invalidates its cache so the test allows up to one minute
// difference in the last modified time.
const MAX_TIME_DIFFERENCE = 60000;

var gTestFiles = [
{
  fileName         : "1_exe1.exe",
  destinationDir   : APPLY_TO_DIR + "/mar_test/1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/aus-0111_general_ref_image.png",
  compareFile      : "data/aus-0110_general_ref_image.png",
  originalPerms    : 0777,
  comparePerms     : 0755
}, {
  fileName         : "1_1_image1.png",
  destinationDir   : APPLY_TO_DIR + "/mar_test/1/1_1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "data/aus-0110_general_ref_image.png",
  originalPerms    : 0776,
  comparePerms     : 0644
}, {
  fileName         : "1_1_text1",
  destinationDir   : APPLY_TO_DIR + "/mar_test/1/1_1/",
  originalContents : "ToBeReplacedWithToBeModified\n",
  compareContents  : "ToBeModified\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0775,
  comparePerms     : 0644
}, {
  fileName         : "1_1_text2",
  destinationDir   : APPLY_TO_DIR + "/mar_test/1/1_1/",
  originalContents : "ToBeReplacedWithToBeDeleted\n",
  compareContents  : "ToBeDeleted\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0677,
  comparePerms     : 0644
}, {
  fileName         : "2_1_text1",
  destinationDir   : APPLY_TO_DIR + "/mar_test/2/2_1/",
  originalContents : "ToBeReplacedWithToBeDeleted\n",
  compareContents  : "ToBeDeleted\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0767,
  comparePerms     : 0644
}];

function run_test() {
  if (IS_ANDROID) {
    logTestInfo("this test is not applicable to Android... returning early");
    return;
  }

  do_test_pending();
  do_register_cleanup(end_test);

  var testDir, testFile;

  // The directory the updates will be applied to is the directory name stored
  // in the APPLY_TO_DIR constant located in the current working directory and
  // not dist/bin.
  var applyToDir = do_get_file(APPLY_TO_DIR, true);

  // Remove the directory where the update will be applied if it exists.
  try {
    removeDirRecursive(applyToDir);
  }
  catch (e) {
    dump("Unable to remove directory\n" +
         "path: " + applyToDir.path + "\n" +
         "Exception: " + e + "\n");
  }
  logTestInfo("testing successful removal of the directory used to apply the " +
              "mar file");
  do_check_false(applyToDir.exists());

  // Use a directory outside of dist/bin to lessen the garbage in dist/bin
  var updatesDir = do_get_file(UPDATES_DIR, true);
  try {
    // Mac OS X intermittently fails when removing the dir where the updater
    // binary was launched.
    removeDirRecursive(updatesDir);
  }
  catch (e) {
    dump("Unable to remove directory\n" +
         "path: " + updatesDir.path + "\n" +
         "Exception: " + e + "\n");
  }
  if (!updatesDir.exists()) {
    updatesDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
  }

  // Create the files to test the complete mar's ability to replace files.
  for (var i = 0; i < gTestFiles.length; i++) {
    var f = gTestFiles[i];
    if (f.originalFile || f.originalContents) {
      testDir = do_get_file(f.destinationDir, true);
      if (!testDir.exists())
        testDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);

      if (f.originalFile) {
        testFile = do_get_file(f.originalFile);
        testFile.copyTo(testDir, f.fileName);
        testFile = do_get_file(f.destinationDir + f.fileName);
      }
      else {
        testFile = do_get_file(f.destinationDir + f.fileName, true);
        writeFile(testFile, f.originalContents);
      }

      // Skip these tests on Windows (includes WinCE) and OS/2 since their
      // implementaions of chmod doesn't really set permissions.
      if (!IS_WIN && !IS_OS2 && f.originalPerms) {
        testFile.permissions = f.originalPerms;
        // Store the actual permissions on the file for reference later after
        // setting the permissions.
        if (!f.comparePerms)
          f.comparePerms = testFile.permissions;
      }
    }
  }

  var afterApplyBinDir = applyToDir.clone();
  afterApplyBinDir.append(AFTER_APPLY_DIR);

  var afterApplyBin = do_get_file(AFTER_APPLY_BIN_FILE);
  afterApplyBin.copyTo(afterApplyBinDir, RELAUNCH_BIN_FILE);

  var relaunchApp = afterApplyBinDir.clone();
  relaunchApp.append(RELAUNCH_BIN_FILE);
  relaunchApp.permissions = PERMS_DIRECTORY;

  let updaterIniContents = "[Strings]\n" +
                           "Title=Update Test\n" +
                           "Info=Application Update XPCShell Test - " +
                           "test_0110_general.js\n";
  var updaterIni = updatesDir.clone();
  updaterIni.append(FILE_UPDATER_INI);
  writeFile(updaterIni, updaterIniContents);
  updaterIni.copyTo(afterApplyBinDir, FILE_UPDATER_INI);

  // For Mac OS X set the last modified time for the root directory to a date in
  // the past to test that the last modified time is updated on a successful
  // update (bug 600098).
  if (IS_MACOSX) {
    var now = Date.now();
    var yesterday = now - (1000 * 60 * 60 * 24);
    applyToDir.lastModifiedTime = yesterday;
  }

  var binDir = getGREDir();

  // The updater binary file
  var updater = binDir.clone();
  updater.append("updater.app");
  if (!updater.exists()) {
    updater = binDir.clone();
    updater.append(UPDATER_BIN_FILE);
    if (!updater.exists()) {
      do_throw("Unable to find updater binary!");
    }
  }

  var mar = do_get_file("data/aus-0110_general.mar");
  mar.copyTo(updatesDir, FILE_UPDATE_ARCHIVE);

  // apply the complete mar and check the innards of the files
  var exitValue = runUpdate(updater, updatesDir, applyToDir, relaunchApp,
                            RELAUNCH_ARGS);
  logTestInfo("testing updater binary process exitValue for success when " +
              "applying a complete mar");
  do_check_eq(exitValue, 0);

  logTestInfo("testing update.status should be " + STATE_SUCCEEDED);
  do_check_eq(readStatusFile(updatesDir), STATE_SUCCEEDED);

  // For Mac OS X check that the last modified time for a directory has been
  // updated after a successful update (bug 600098).
  if (IS_MACOSX) {
    logTestInfo("testing last modified time on the apply to directory has " +
                "changed after a successful update (bug 600098)");
    now = Date.now();
    var timeDiff = Math.abs(applyToDir.lastModifiedTime - now);
    do_check_true(timeDiff < MAX_TIME_DIFFERENCE);
  }

  logTestInfo("testing contents of files added by a complete mar");
  for (i = 0; i < gTestFiles.length; i++) {
    f = gTestFiles[i];
    testFile = do_get_file(f.destinationDir + f.fileName, true);
    logTestInfo("testing file: " + testFile.path);
    if (f.compareFile || f.compareContents) {
      do_check_true(testFile.exists());

      // Skip these tests on Windows (includes WinCE) and OS/2 since their
      // implementaions of chmod doesn't really set permissions.
      if (!IS_WIN && !IS_OS2 && f.comparePerms) {
        // Check if the permssions as set in the complete mar file are correct.
        let logPerms = "testing file permissions - ";
        if (f.originalPerms) {
          logPerms += "original permissions: " + f.originalPerms.toString(8) + ", ";
        }
        logPerms += "compare permissions : " + f.comparePerms.toString(8) + ", ";
        logPerms += "updated permissions : " + testFile.permissions.toString(8);
        logTestInfo(logPerms);
        do_check_eq(testFile.permissions & 0xfff, f.comparePerms & 0xfff);
      }

      if (f.compareFile) {
        do_check_eq(readFileBytes(testFile),
                    readFileBytes(do_get_file(f.compareFile)));
        if (f.originalFile) {
          // Verify that readFileBytes returned the entire contents by checking
          // the contents against the original file.
          do_check_neq(readFileBytes(testFile),
                       readFileBytes(do_get_file(f.originalFile)));
        }
      }
      else {
        do_check_eq(readFileBytes(testFile), f.compareContents);
      }
    }
    else {
      do_check_false(testFile.exists());
    }
  }

  logTestInfo("testing patch files should not be left behind");
  var entries = updatesDir.QueryInterface(AUS_Ci.nsIFile).directoryEntries;
  while (entries.hasMoreElements()) {
    var entry = entries.getNext().QueryInterface(AUS_Ci.nsIFile);
    do_check_neq(getFileExtension(entry), "patch");
  }

  check_app_launch_log();
}

function end_test() {
  // Try to remove the updates and the apply to directories.
  var applyToDir = do_get_file(APPLY_TO_DIR, true);
  try {
    removeDirRecursive(applyToDir);
  }
  catch (e) {
    dump("Unable to remove directory\n" +
         "path: " + applyToDir.path + "\n" +
         "Exception: " + e + "\n");
  }

  var updatesDir = do_get_file(UPDATES_DIR, true);
  try {
    removeDirRecursive(updatesDir);
  }
  catch (e) {
    dump("Unable to remove directory\n" +
         "path: " + updatesDir.path + "\n" +
         "Exception: " + e + "\n");
  }

  cleanUp();
}

function check_app_launch_log() {
  var appLaunchLog = do_get_file(APPLY_TO_DIR);
  appLaunchLog.append(AFTER_APPLY_DIR);
  appLaunchLog.append(RELAUNCH_BIN_FILE + ".log");
  if (!appLaunchLog.exists()) {
    do_timeout(0, check_app_launch_log);
    return;
  }

  var expectedLogContents = "executed\n" + RELAUNCH_ARGS.join("\n") + "\n";
  var logContents = readFile(appLaunchLog).replace(/\r\n/g, "\n");
  // It is possible for the log file contents check to occur before the log file
  // contents are completely written so wait until the contents are the expected
  // value. If the contents are never the expected value then the test will
  // fail by timing out.
  if (logContents != expectedLogContents) {
    do_timeout(0, check_app_launch_log);
    return;
  }

  logTestInfo("testing that the callback application successfully launched " +
              "and the expected command line arguments passed to it");
  do_check_eq(logContents, expectedLogContents);

  do_test_finished();
}
