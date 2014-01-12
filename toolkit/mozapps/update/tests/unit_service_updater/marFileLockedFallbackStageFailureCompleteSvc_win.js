/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File locked complete MAR file staged patch apply failure fallback test */

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
  description      : "Not added for failed update (add)",
  fileName         : "precomplete",
  relPathDir       : "",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial_precomplete",
  compareFile      : "partial_precomplete"
}, {
  description      : "Not added for failed update (add)",
  fileName         : "searchpluginstext0",
  relPathDir       : "a/b/searchplugins/",
  originalContents : "ShouldNotBeReplaced\n",
  compareContents  : "ShouldNotBeReplaced\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Not added for failed update (add)",
  fileName         : "searchpluginspng1.png",
  relPathDir       : "a/b/searchplugins/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial.png",
  compareFile      : "partial.png"
}, {
  description      : "Not added for failed update (add)",
  fileName         : "searchpluginspng0.png",
  relPathDir       : "a/b/searchplugins/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial.png",
  compareFile      : "partial.png"
}, {
  description      : "Not added for failed update (add)",
  fileName         : "removed-files",
  relPathDir       : "a/b/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial_removed-files",
  compareFile      : "partial_removed-files"
}, {
  description      : "Not added for failed update (add-if)",
  fileName         : "extensions1text0",
  relPathDir       : "a/b/extensions/extensions1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Not added for failed update (add-if)",
  fileName         : "extensions1png1.png",
  relPathDir       : "a/b/extensions/extensions1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Not added for failed update (add-if)",
  fileName         : "extensions1png0.png",
  relPathDir       : "a/b/extensions/extensions1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Not added for failed update (add-if)",
  fileName         : "extensions0text0",
  relPathDir       : "a/b/extensions/extensions0/",
  originalContents : "ShouldNotBeReplaced\n",
  compareContents  : "ShouldNotBeReplaced\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Not added for failed update (add-if)",
  fileName         : "extensions0png1.png",
  relPathDir       : "a/b/extensions/extensions0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial.png",
  compareFile      : "partial.png"
}, {
  description      : "Not added for failed update (add-if)",
  fileName         : "extensions0png0.png",
  relPathDir       : "a/b/extensions/extensions0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Not added for failed update (add)",
  fileName         : "exe0.exe",
  relPathDir       : "a/b/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial.png",
  compareFile      : "partial.png"
}, {
  description      : "Not added for failed update (add)",
  fileName         : "10text0",
  relPathDir       : "a/b/1/10/",
  originalContents : "ShouldNotBeReplaced\n",
  compareContents  : "ShouldNotBeReplaced\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Not added for failed update (add)",
  fileName         : "0exe0.exe",
  relPathDir       : "a/b/0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Not added for failed update (add)",
  fileName         : "00text1",
  relPathDir       : "a/b/0/00/",
  originalContents : "ShouldNotBeReplaced\n",
  compareContents  : "ShouldNotBeReplaced\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Not added for failed update (add)",
  fileName         : "00text0",
  relPathDir       : "a/b/0/00/",
  originalContents : "ShouldNotBeReplaced\n",
  compareContents  : "ShouldNotBeReplaced\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Not added for failed update (add)",
  fileName         : "00png0.png",
  relPathDir       : "a/b/0/00/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial.png",
  compareFile      : "partial.png"
}, {
  description      : "Not removed for failed update (remove)",
  fileName         : "20text0",
  relPathDir       : "a/b/2/20/",
  originalContents : "ShouldNotBeDeleted\n",
  compareContents  : "ShouldNotBeDeleted\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Not removed for failed update (remove)",
  fileName         : "20png0.png",
  relPathDir       : "a/b/2/20/",
  originalContents : "ShouldNotBeDeleted\n",
  compareContents  : "ShouldNotBeDeleted\n",
  originalFile     : null,
  compareFile      : null
}];

ADDITIONAL_TEST_DIRS = [
{
  description  : "Not removed for failed update (rmdir)",
  relPathDir   : "a/b/2/20/",
  dirRemoved   : false
}, {
  description  : "Not removed for failed update (rmdir)",
  relPathDir   : "a/b/2/",
  dirRemoved   : false
}];

function run_test() {
  if (!shouldRunServiceTest()) {
    return;
  }

  gStageUpdate = true;
  setupTestCommon();
  setupUpdaterTest(FILE_COMPLETE_MAR);

  // Exclusively lock an existing file so it is in use during the update.
  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let helperDestDir = getApplyDirFile("a/b/");
  helperBin.copyTo(helperDestDir, FILE_HELPER_BIN);
  helperBin = getApplyDirFile("a/b/" + FILE_HELPER_BIN);
  // Strip off the first two directories so the path has to be from the helper's
  // working directory.
  let lockFileRelPath = TEST_FILES[3].relPathDir.split("/");
  lockFileRelPath = lockFileRelPath.slice(2);
  lockFileRelPath = lockFileRelPath.join("/") + "/" + TEST_FILES[3].fileName;
  let args = [getApplyDirPath() + "a/b/", "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT, lockFileRelPath];
  let lockFileProcess = AUS_Cc["@mozilla.org/process/util;1"].
                     createInstance(AUS_Ci.nsIProcess);
  lockFileProcess.init(helperBin);
  lockFileProcess.run(false, args, args.length);

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
  runUpdateUsingService(STATE_PENDING_SVC, STATE_FAILED);
}

function checkUpdateFinished() {
  logTestInfo("testing update.status should be " + STATE_FAILED);
  do_check_eq(readStatusState(), STATE_FAILED);

  // Now switch the application and its updated version.
  gStageUpdate = false;
  gSwitchApp = true;
  runUpdate(1, STATE_PENDING);
}

function checkUpdateApplied() {
  setupHelperFinish();
}

function checkUpdate() {
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(ERR_RENAME_FILE);

  logTestInfo("testing tobedeleted directory doesn't exist");
  let toBeDeletedDir = getApplyDirFile("tobedeleted", true);
  do_check_false(toBeDeletedDir.exists());

  checkCallbackAppLog();
}
