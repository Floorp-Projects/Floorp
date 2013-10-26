/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File in use inside removed dir complete MAR file patch apply success test */

// The files are listed in the same order as they are applied from the mar's
// update.manifest. Complete updates have remove file and rmdir directory
// operations located in the precomplete file performed first.
const TEST_FILES = [
{
  description      : "Should never change",
  fileName         : "channel-prefs.js",
  relPathDir       : "a/b/defaults/pref/",
  originalContents : "ShouldNotBeReplaced\n",
  compareContents  : "ShouldNotBeReplaced\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "precomplete",
  relPathDir       : "",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial_precomplete",
  compareFile      : "complete_precomplete"
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "searchpluginstext0",
  relPathDir       : "a/b/searchplugins/",
  originalContents : "ToBeReplacedWithFromComplete\n",
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "searchpluginspng1.png",
  relPathDir       : "a/b/searchplugins/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "complete.png"
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "searchpluginspng0.png",
  relPathDir       : "a/b/searchplugins/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial.png",
  compareFile      : "complete.png"
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "removed-files",
  relPathDir       : "a/b/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial_removed-files",
  compareFile      : "complete_removed-files"
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions1text0",
  relPathDir       : "a/b/extensions/extensions1/",
  originalContents : null,
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions1png1.png",
  relPathDir       : "a/b/extensions/extensions1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial.png",
  compareFile      : "complete.png"
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions1png0.png",
  relPathDir       : "a/b/extensions/extensions1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "complete.png"
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions0text0",
  relPathDir       : "a/b/extensions/extensions0/",
  originalContents : "ToBeReplacedWithFromComplete\n",
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions0png1.png",
  relPathDir       : "a/b/extensions/extensions0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "complete.png"
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions0png0.png",
  relPathDir       : "a/b/extensions/extensions0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "complete.png"
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "exe0.exe",
  relPathDir       : "a/b/",
  originalContents : null,
  compareContents  : null,
  originalFile     : FILE_HELPER_BIN,
  compareFile      : "complete.png"
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "10text0",
  relPathDir       : "a/b/1/10/",
  originalContents : "ToBeReplacedWithFromComplete\n",
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Added by update.manifest (add) file in use",
  fileName         : "0exe0.exe",
  relPathDir       : "a/b/0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : FILE_HELPER_BIN,
  compareFile      : "complete.png"
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "00text1",
  relPathDir       : "a/b/0/00/",
  originalContents : "ToBeReplacedWithFromComplete\n",
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "00text0",
  relPathDir       : "a/b/0/00/",
  originalContents : "ToBeReplacedWithFromComplete\n",
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "00png0.png",
  relPathDir       : "a/b/0/00/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "complete.png"
}, {
  description      : "Removed by precomplete (remove)",
  fileName         : "20text0",
  relPathDir       : "a/b/2/20/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Removed by precomplete (remove)",
  fileName         : "20png0.png",
  relPathDir       : "a/b/2/20/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}];

ADDITIONAL_TEST_DIRS = [
{
  description  : "Removed by precomplete (rmdir)",
  relPathDir   : "a/b/2/20/",
  dirRemoved   : true
}, {
  description  : "Removed by precomplete (rmdir)",
  relPathDir   : "a/b/2/",
  dirRemoved   : true
}];

function run_test() {
  setupTestCommon(true);

  setupUpdaterTest(FILE_COMPLETE_MAR);

  let fileInUseBin = getApplyDirFile(TEST_DIRS[4].relPathDir +
                                     TEST_DIRS[4].subDirs[0] +
                                     TEST_DIRS[4].subDirFiles[0]);
  // Remove the empty file created for the test so the helper application can
  // replace it.
  fileInUseBin.remove(false);

  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let fileInUseDir = getApplyDirFile(TEST_DIRS[4].relPathDir +
                                    TEST_DIRS[4].subDirs[0]);
  helperBin.copyTo(fileInUseDir, TEST_DIRS[4].subDirFiles[0]);

  // Launch an existing file so it is in use during the update
  let args = [getApplyDirPath() + "a/b/", "input", "output", "-s", "20"];
  let fileInUseProcess = AUS_Cc["@mozilla.org/process/util;1"].
                         createInstance(AUS_Ci.nsIProcess);
  fileInUseProcess.init(fileInUseBin);
  fileInUseProcess.run(false, args, args.length);

  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
  // apply the complete mar
  let exitValue = runUpdate();
  logTestInfo("testing updater binary process exitValue for success when " +
              "applying a complete mar");
  do_check_eq(exitValue, 0);

  setupHelperFinish();
}

function checkUpdate() {
  logTestInfo("testing update.status should be " + STATE_SUCCEEDED);
  let updatesDir = do_get_file(gTestID + UPDATES_DIR_SUFFIX);
  do_check_eq(readStatusFile(updatesDir), STATE_SUCCEEDED);

  checkFilesAfterUpdateSuccess();
  checkUpdateLogContains(ERR_BACKUP_DISCARD);

  logTestInfo("testing tobedeleted directory exists");
  let toBeDeletedDir = getApplyDirFile("tobedeleted", true);
  do_check_true(toBeDeletedDir.exists());

  checkCallbackAppLog();
}

function end_test() {
  cleanupUpdaterTest();
}
