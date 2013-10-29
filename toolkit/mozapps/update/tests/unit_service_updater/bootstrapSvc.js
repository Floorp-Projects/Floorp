/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Bootstrap the tests using the service by installing our own version of the service */

const TEST_FILES = [
{
  description      : "the dummy file to make sure that the update worked",
  fileName         : "dummy",
  relPathDir       : "/",
  originalContents : null,
  compareContents  : "",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : null,
  comparePerms     : null
}
];

function run_test() {
  if (!shouldRunServiceTest(true)) {
    return;
  }

  setupTestCommon(false);
  do_register_cleanup(cleanupUpdaterTest);

  setupUpdaterTest(FILE_COMPLETE_MAR);

  // apply the complete mar
  runUpdateUsingService(STATE_PENDING_SVC, STATE_SUCCEEDED, checkUpdateApplied, null, false);
}

function checkUpdateApplied() {
  checkFilesAfterUpdateSuccess();

  // We need to check the service log even though this is a bootstrap
  // because the app bin could be in use by this test by the time the next
  // test runs.
  checkCallbackServiceLog();
}
