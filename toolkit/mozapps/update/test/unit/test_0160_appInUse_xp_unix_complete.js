/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Application in use complete MAR file patch apply success test */

const TEST_ID = "0160";
// All we care about is that the last modified time has changed so that Mac OS
// X Launch Services invalidates its cache so the test allows up to one minute
// difference in the last modified time.
const MAX_TIME_DIFFERENCE = 60000;

// The files are in the same order as they are applied from the mar
const TEST_FILES = [
{
  fileName         : "00png0.png",
  relPathDir       : "0/00/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "data/complete.png",
  originalPerms    : 0776,
  comparePerms     : 0644
}, {
  fileName         : "00text0",
  relPathDir       : "0/00/",
  originalContents : "ToBeReplacedWithToBeModified\n",
  compareContents  : "ToBeModified\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0775,
  comparePerms     : 0644
}, {
  fileName         : "00text0",
  relPathDir       : "0/00/",
  originalContents : "ToBeReplacedWithToBeDeleted\n",
  compareContents  : "ToBeDeleted\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0677,
  comparePerms     : 0644
}, {
  fileName         : "0exe0.exe",
  relPathDir       : "0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/partial.png",
  compareFile      : "data/complete.png",
  originalPerms    : 0777,
  comparePerms     : 0755
}, {
  fileName         : "10text0",
  relPathDir       : "1/10/",
  originalContents : "ToBeReplacedWithToBeDeleted\n",
  compareContents  : "ToBeDeleted\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0767,
  comparePerms     : 0644
}, {
  fileName         : "exe0.exe",
  relPathDir       : "",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/partial.png",
  compareFile      : "data/complete.png",
  originalPerms    : 0777,
  comparePerms     : 0755
}];

function run_test() {
  if (!IS_UNIX || IS_ANDROID) {
    logTestInfo("this test is only applicable to XP_UNIX platforms except " +
                "for Android... returning early");
    return;
  }

  do_test_pending();
  do_register_cleanup(cleanupUpdaterTest);

  setupUpdaterTest(MAR_COMPLETE_FILE);

  // Launch the callback helper application so it is in use during the update
  let callbackApp = getApplyDirFile(CALLBACK_BIN_FILE);
  callbackApp.permissions = PERMS_DIRECTORY;
  let args = [getApplyDirPath(), "input", "output", "-s", "20"];
  let callbackAppProcess = AUS_Cc["@mozilla.org/process/util;1"].
                           createInstance(AUS_Ci.nsIProcess);
  callbackAppProcess.init(callbackApp);
  callbackAppProcess.run(false, args, args.length);

  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
  let applyToDir = getApplyDirFile();

  // For Mac OS X set the last modified time for the root directory to a date in
  // the past to test that the last modified time is updated on a successful
  // update (bug 600098).
  if (IS_MACOSX) {
    let now = Date.now();
    let yesterday = now - (1000 * 60 * 60 * 24);
    applyToDir.lastModifiedTime = yesterday;
  }

  // apply the complete mar
  let exitValue = runUpdate();
  logTestInfo("testing updater binary process exitValue for success when " +
              "applying a complete mar");
  do_check_eq(exitValue, 0);

  setupHelperFinish();
}


function checkUpdate() {
  let updatesDir = do_get_file(TEST_ID + UPDATES_DIR_SUFFIX);
  let applyToDir = getApplyDirFile();

  logTestInfo("testing update.status should be " + STATE_SUCCEEDED);
  do_check_eq(readStatusFile(updatesDir), STATE_SUCCEEDED);

  // For Mac OS X check that the last modified time for a directory has been
  // updated after a successful update (bug 600098).
  if (IS_MACOSX) {
    logTestInfo("testing last modified time on the apply to directory has " +
                "changed after a successful update (bug 600098)");
    let now = Date.now();
    let timeDiff = Math.abs(applyToDir.lastModifiedTime - now);
    do_check_true(timeDiff < MAX_TIME_DIFFERENCE);
  }

  checkFilesAfterUpdateSuccess();

  checkCallbackAppLog();
}
