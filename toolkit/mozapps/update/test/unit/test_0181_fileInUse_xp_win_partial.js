/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File in use partial MAR file patch apply success test */

const TEST_ID = "0181";
const MAR_IN_USE_WIN_FILE = "data/partial_in_use_win.mar";

// The files are in the same order as they are applied from the mar
var TEST_FILES = [
{
  fileName         : "1_1_image1.png",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/1_1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/complete.png",
  compareFile      : "data/partial.png"
}, {
  fileName         : "1_1_text1",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/1_1/",
  originalContents : "ToBeModified\n",
  compareContents  : "Modified\n",
  originalFile     : null,
  compareFile      : null
}, {
  fileName         : "1_1_text2",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/1_1/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}, {
  fileName         : "1_exe1.exe",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/partial_in_use_win_before.exe",
  compareFile      : "data/partial_in_use_win_after.exe"
}, {
  fileName         : "2_1_text1",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/2/2_1/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null
}, {
  fileName         : "1_1_text3",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/1_1/",
  originalContents : null,
  compareContents  : "Added\n",
  originalFile     : null,
  compareFile      : null
}, {
  fileName         : "3_1_text1",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/3/3_1/",
  originalContents : null,
  compareContents  : "Added\n",
  originalFile     : null,
  compareFile      : null
}];

let gFileInUseProcess;

function run_test() {
  if (!IS_WIN || IS_WINCE) {
    logTestInfo("this test is only applicable to Windows... returning early");
    return;
  }

  do_test_pending();
  do_register_cleanup(end_test);

  setupUpdaterTest(TEST_ID, MAR_IN_USE_WIN_FILE, TEST_FILES);

  // Launch an existing file so it is in use during the update
  let fileInUseBin = do_get_file(TEST_FILES[3].destinationDir +
                                 TEST_FILES[3].fileName);
  let args = ["-s", "20"];
  gFileInUseProcess = AUS_Cc["@mozilla.org/process/util;1"].
                      createInstance(AUS_Ci.nsIProcess);
  gFileInUseProcess.init(fileInUseBin);
  gFileInUseProcess.run(false, args, args.length);

  // Give the file in use process time to launch before updating otherwise this
  // test can fail intermittently on Windows debug builds.
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

  gFileInUseProcess.kill();

  checkFilesAfterUpdateSuccess(TEST_ID, TEST_FILES);

  logTestInfo("testing directory still exists after removal of the last file " +
              "in the directory (bug 386760)");
  let testDir = do_get_file(TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/2/2_1/", true);
  do_check_true(testDir.exists());

  logTestInfo("testing tobedeleted directory exists");
  let toBeDeletedDir = applyToDir.clone();
  toBeDeletedDir.append("tobedeleted");
  do_check_true(toBeDeletedDir.exists());

  checkCallbackAppLog(TEST_ID);
}
