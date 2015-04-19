/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Application in use complete MAR file patch apply success test */

function run_test() {
  setupTestCommon();
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setupUpdaterTest(FILE_COMPLETE_MAR);

  // Launch the callback helper application so it is in use during the update.
  let callbackApp = getApplyDirFile(DIR_RESOURCES + gCallbackBinFile);
  callbackApp.permissions = PERMS_DIRECTORY;
  let args = [getApplyDirPath() + DIR_RESOURCES, "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT];
  let callbackAppProcess = Cc["@mozilla.org/process/util;1"].
                           createInstance(Ci.nsIProcess);
  callbackAppProcess.init(callbackApp);
  callbackAppProcess.run(false, args, args.length);

  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
  setAppBundleModTime();
  runUpdate(0, STATE_SUCCEEDED, checkUpdateFinished);
}

function checkUpdateFinished() {
  setupHelperFinish();
}

function checkUpdate() {
  checkAppBundleModTime();
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, false);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS);
  standardInit();
  checkCallbackAppLog();
}
