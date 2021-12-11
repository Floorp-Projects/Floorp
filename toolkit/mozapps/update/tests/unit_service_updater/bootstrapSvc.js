/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Bootstrap the tests using the service by installing our own version of the service */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  // We don't actually care if the MAR has any data, we only care about the
  // application return code and update.status result.
  gTestFiles = gTestFilesCommon;
  gTestDirs = [];
  await setupUpdaterTest(FILE_COMPLETE_MAR, null);
  runUpdate(STATE_SUCCEEDED, false, 0, true);
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, false);

  // We need to check the service log even though this is a bootstrap
  // because the app bin could be in use by this test by the time the next
  // test runs.
  checkCallbackLog();
}
