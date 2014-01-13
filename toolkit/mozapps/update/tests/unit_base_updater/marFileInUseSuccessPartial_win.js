/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File in use partial MAR file patch apply success test */

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
  originalFile     : "complete_precomplete",
  compareFile      : "partial_precomplete"
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "searchpluginstext0",
  relPathDir       : "a/b/searchplugins/",
  originalContents : "ToBeReplacedWithFromPartial\n",
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Patched by update.manifest if the file exists " +
                     "(patch-if)",
  fileName         : "searchpluginspng1.png",
  relPathDir       : "a/b/searchplugins/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png"
}, {
  description      : "Patched by update.manifest if the file exists " +
                     "(patch-if)",
  fileName         : "searchpluginspng0.png",
  relPathDir       : "a/b/searchplugins/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png"
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions1text0",
  relPathDir       : "a/b/extensions/extensions1/",
  originalContents : null,
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Patched by update.manifest if the parent directory " +
                     "exists (patch-if)",
  fileName         : "extensions1png1.png",
  relPathDir       : "a/b/extensions/extensions1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png"
}, {
  description      : "Patched by update.manifest if the parent directory " +
                     "exists (patch-if)",
  fileName         : "extensions1png0.png",
  relPathDir       : "a/b/extensions/extensions1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png"
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions0text0",
  relPathDir       : "a/b/extensions/extensions0/",
  originalContents : "ToBeReplacedWithFromPartial\n",
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Patched by update.manifest if the parent directory " +
                     "exists (patch-if)",
  fileName         : "extensions0png1.png",
  relPathDir       : "a/b/extensions/extensions0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png"
}, {
  description      : "Patched by update.manifest if the parent directory " +
                     "exists (patch-if)",
  fileName         : "extensions0png0.png",
  relPathDir       : "a/b/extensions/extensions0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png"
}, {
  description      : "Patched by update.manifest (patch)",
  fileName         : "exe0.exe",
  relPathDir       : "a/b/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial_in_use_win_before.exe",
  compareFile      : "partial_in_use_win_after.exe"
}, {
  description      : "Patched by update.manifest (patch) file in use",
  fileName         : "0exe0.exe",
  relPathDir       : "a/b/0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial_in_use_win_before.exe",
  compareFile      : "partial_in_use_win_after.exe"
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "00text0",
  relPathDir       : "a/b/0/00/",
  originalContents : "ToBeReplacedWithFromPartial\n",
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Patched by update.manifest (patch)",
  fileName         : "00png0.png",
  relPathDir       : "a/b/0/00/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png"
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "20text0",
  relPathDir       : "a/b/2/20/",
  originalContents : null,
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "20png0.png",
  relPathDir       : "a/b/2/20/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "partial.png"
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "00text2",
  relPathDir       : "a/b/0/00/",
  originalContents : null,
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Removed by update.manifest (remove)",
  fileName         : "10text0",
  relPathDir       : "a/b/1/10/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}, {
  description      : "Removed by update.manifest (remove)",
  fileName         : "00text1",
  relPathDir       : "a/b/0/00/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}];

ADDITIONAL_TEST_DIRS = [
{
  description  : "Removed by update.manifest (rmdir)",
  relPathDir   : "a/b/1/10/",
  dirRemoved   : true
}, {
  description  : "Removed by update.manifest (rmdir)",
  relPathDir   : "a/b/1/",
  dirRemoved   : true
}];

function run_test() {
  setupTestCommon();
  setupUpdaterTest(FILE_PARTIAL_WIN_MAR);

  // Launch an existing file so it is in use during the update.
  let fileInUseBin = getApplyDirFile(TEST_FILES[12].relPathDir +
                                     TEST_FILES[12].fileName);
  let args = [getApplyDirPath() + "a/b/", "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT];
  let fileInUseProcess = AUS_Cc["@mozilla.org/process/util;1"].
                         createInstance(AUS_Ci.nsIProcess);
  fileInUseProcess.init(fileInUseBin);
  fileInUseProcess.run(false, args, args.length);

  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
  runUpdate(0, STATE_SUCCEEDED);
}

function checkUpdateApplied() {
  setupHelperFinish();
}

function checkUpdate() {
  checkFilesAfterUpdateSuccess();
  checkUpdateLogContains(ERR_BACKUP_DISCARD);

  logTestInfo("testing tobedeleted directory exists");
  let toBeDeletedDir = getApplyDirFile("tobedeleted", true);
  do_check_true(toBeDeletedDir.exists());

  checkCallbackAppLog();
}
