/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Patch app binary partial MAR file patch apply success test */

function run_test() {
  if (!shouldRunServiceTest()) {
    return;
  }

  setupTestCommon();
  gTestFiles = gTestFilesPartialSuccess;
  gTestDirs = gTestDirsPartialSuccess;
  setupUpdaterTest(FILE_PARTIAL_MAR, false, false);

  gCallbackBinFile = "exe0.exe";

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  runUpdateUsingService(STATE_PENDING_SVC, STATE_SUCCEEDED);
}

function checkUpdateFinished() {
  checkFilesAfterUpdateSuccess();
  checkCallbackServiceLog();
}
