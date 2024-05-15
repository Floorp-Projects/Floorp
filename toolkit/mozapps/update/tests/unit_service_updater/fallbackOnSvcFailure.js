/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * If updating with the maintenance service fails in a way specific to the
 * maintenance service, we should fall back to not using the maintenance
 * service, which should succeed. This test ensures that that happens as
 * expected.
 */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  // This variable forces the service to fail by having the updater pass it the
  // wrong number of arguments. Then we can verify that the fallback happens
  // properly.
  gEnvForceServiceFallback = true;

  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  preventDistributionFiles();
  await setupUpdaterTest(FILE_COMPLETE_MAR, true);
  // It's very important that we pass in true for aCheckSvcLog (4th param),
  // because otherwise we may not have used the service at all, so we wouldn't
  // really check that we fell back (to not using the service) properly.
  runUpdate(STATE_SUCCEEDED, false, 0, true);
  await checkPostUpdateAppLog();
  checkAppBundleModTime();
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);
  checkCallbackLog();
}
