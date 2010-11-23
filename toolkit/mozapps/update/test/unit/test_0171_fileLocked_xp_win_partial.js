/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File locked partial MAR file patch apply failure test */

const TEST_ID = "0171";

// The files are in the same order as they are applied from the mar
const TEST_FILES = [
{
  fileName         : "1_1_image1.png",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/1_1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/partial.png",
  compareFile      : "data/partial.png"
}, {
  fileName         : "1_1_text1",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/1_1/",
  originalContents : "ShouldNotBeModified\n",
  compareContents  : "ShouldNotBeModified\n",
  originalFile     : null,
  compareFile      : null
}, {
  fileName         : "1_1_text2",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/1_1/",
  originalContents : "ShouldNotBeDeleted\n",
  compareContents  : "ShouldNotBeDeleted\n",
  originalFile     : null,
  compareFile      : null
}, {
  fileName         : "1_exe1.exe",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/complete.png",
  compareFile      : "data/complete.png"
}, {
  fileName         : "2_1_text1",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/2/2_1/",
  originalContents : "ShouldNotBeDeleted\n",
  compareContents  : "ShouldNotBeDeleted\n",
  originalFile     : null,
  compareFile      : null
}];

let gLockFileProcess;

function run_test() {
  if (!IS_WIN || IS_WINCE) {
    logTestInfo("this test is only applicable to Windows... returning early");
    return;
  }

  do_test_pending();
  do_register_cleanup(end_test);

  setupUpdaterTest(TEST_ID, MAR_PARTIAL_FILE, TEST_FILES);

  // Exclusively lock an existing file so it is in use during the update
  let helperBin = do_get_file(HELPER_BIN_FILE);
  let lockFileRelPath = TEST_FILES[3].destinationDir + TEST_FILES[3].fileName;
  let args = ["-s", "20", lockFileRelPath];
  gLockFileProcess = AUS_Cc["@mozilla.org/process/util;1"].
                     createInstance(AUS_Ci.nsIProcess);
  gLockFileProcess.init(helperBin);
  gLockFileProcess.run(false, args, args.length);

  // Give the lock file process time to lock the file before updating otherwise
  // this test can fail intermittently on Windows debug builds.
  do_timeout(100, testUpdate);
}

function end_test() {
  cleanupUpdaterTest(TEST_ID);
}

function testUpdate() {
  let updatesDir = do_get_file(TEST_ID + UPDATES_DIR_SUFFIX);
  let applyToDir = do_get_file(TEST_ID + APPLY_TO_DIR_SUFFIX);

  // apply the partial mar
  let exitValue = runUpdate(TEST_ID);
  logTestInfo("testing updater binary process exitValue for success when " +
              "applying a partial mar");
  do_check_eq(exitValue, 0);

  gLockFileProcess.kill();

  logTestInfo("testing update.status should be " + STATE_FAILED);
  // The update status format for a failure is failed: # where # is the error
  // code for the failure.
  do_check_eq(readStatusFile(updatesDir).split(": ")[0], STATE_FAILED);

  checkFilesAfterUpdateFailure(TEST_ID, TEST_FILES);

  logTestInfo("testing tobedeleted directory doesn't exist");
  let toBeDeletedDir = applyToDir.clone();
  toBeDeletedDir.append("tobedeleted");
  do_check_false(toBeDeletedDir.exists());

  checkCallbackAppLog(TEST_ID);
}
