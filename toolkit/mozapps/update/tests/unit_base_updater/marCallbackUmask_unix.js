/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Verify that app callback is launched with the same umask as was set
 * before applying an update. */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  preventDistributionFiles();

  // Our callback is `TestAUSHelper check-umask <umask from before updating>`.
  // Including the umask from before updating as an argument allows to re-use
  // the callback log checking code below.  The argument is also used as the log
  // file name, so we prefix it with "umask" so that it doesn't clash with
  // numericfile and directory names in the update data.  In particular, "2"
  // clashes with an existing directory name in the update data, leading to
  // failing tests.
  let umask = Services.sysinfo.getProperty("umask");
  gCallbackArgs = ["check-umask", `umask-${umask}`];

  await setupUpdaterTest(FILE_COMPLETE_MAR, true);
  runUpdate(STATE_SUCCEEDED, false, 0, true);
  await checkPostUpdateAppLog();
  checkAppBundleModTime();
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS, false, false, true);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);

  // This compares the callback arguments given, including the umask before
  // updating, to the umask set when the app callback is launched.  They should
  // be the same.
  checkCallbackLog(getApplyDirFile(DIR_RESOURCES + "callback_app.log"));
}
