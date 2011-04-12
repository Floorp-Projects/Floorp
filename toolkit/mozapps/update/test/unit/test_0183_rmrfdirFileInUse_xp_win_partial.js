/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File in use partial MAR file patch apply success test */

const TEST_ID = "0183";
const MAR_IN_USE_WIN_FILE = "data/partial.mar";

// The files are in the same order as they are applied from the mar
var TEST_FILES = [
{
  fileName         : "00png0.png",
  relPathDir       : "0/00/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/complete.png",
  compareFile      : "data/partial.png"
}, {
  fileName         : "00text0",
  relPathDir       : "0/00/",
  originalContents : "ToBeModified\n",
  compareContents  : "Modified\n",
  originalFile     : null,
  compareFile      : null
}, {
  fileName         : "00text1",
  relPathDir       : "0/00/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}, {
  fileName         : "0exe0.exe",
  relPathDir       : "0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/complete.png",
  compareFile      : "data/partial.png"
}, {
  fileName         : "10text0",
  relPathDir       : "1/10/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}, {
  fileName         : "00text2",
  relPathDir       : "0/00/",
  originalContents : null,
  compareContents  : "Added\n",
  originalFile     : null,
  compareFile      : null
}, {
  fileName         : "20text0",
  relPathDir       : "2/20/",
  originalContents : null,
  compareContents  : "Added\n",
  originalFile     : null,
  compareFile      : null
}, {
  fileName         : "exe0.exe",
  relPathDir       : "",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/complete.png",
  compareFile      : "data/partial.png"
}];

function run_test() {
  if (!IS_WIN || IS_WINCE) {
    logTestInfo("this test is only applicable to Windows... returning early");
    return;
  }

  do_test_pending();
  do_register_cleanup(cleanupUpdaterTest);

  setupUpdaterTest(MAR_IN_USE_WIN_FILE);

  let fileInUseBin = getApplyDirFile(TEST_DIRS[2].relPathDir +
                                     TEST_DIRS[2].files[0]);
  // Remove the empty file created for the test so the helper application can
  // replace it.
  fileInUseBin.remove(false);

  let helperBin = do_get_file(HELPER_BIN_FILE);
  let fileInUseDir = getApplyDirFile(TEST_DIRS[2].relPathDir);
  helperBin.copyTo(fileInUseDir, TEST_DIRS[2].files[0]);

  // Launch an existing file so it is in use during the update
  let args = [getApplyDirPath(), "input", "output", "-s", "20"];
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
              "applying a partial mar");
  do_check_eq(exitValue, 0);

  setupHelperFinish();
}

function checkUpdate() {
  let updatesDir = do_get_file(TEST_ID + UPDATES_DIR_SUFFIX);
  let applyToDir = getApplyDirFile();

  logTestInfo("testing update.status should be " + STATE_SUCCEEDED);
  do_check_eq(readStatusFile(updatesDir), STATE_SUCCEEDED);

  checkFilesAfterUpdateSuccess();

  logTestInfo("testing tobedeleted directory exists");
  let toBeDeletedDir = applyToDir.clone();
  toBeDeletedDir.append("tobedeleted");
  do_check_true(toBeDeletedDir.exists());

  checkCallbackAppLog();
}
