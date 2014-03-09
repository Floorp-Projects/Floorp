/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Partial MAR File Patch Apply Test */

function run_test() {
  if (!shouldRunServiceTest()) {
    return;
  }

  setupTestCommon();
  gTestFiles = gTestFilesPartialSuccess;
  gTestFiles[gTestFiles.length - 1].originalContents = null;
  gTestFiles[gTestFiles.length - 1].compareContents = "FromPartial\n";
  gTestFiles[gTestFiles.length - 1].comparePerms = 0o644;
  gTestFiles[gTestFiles.length - 2].originalContents = null;
  gTestFiles[gTestFiles.length - 2].compareContents = "FromPartial\n";
  gTestFiles[gTestFiles.length - 2].comparePerms = 0o644;
  gTestDirs = gTestDirsPartialSuccess;
  setupUpdaterTest(FILE_PARTIAL_MAR, false, false);

  // For Mac OS X set the last modified time for the root directory to a date in
  // the past to test that the last modified time is updated on all updates since
  // the precomplete file in the root of the bundle is renamed, etc. (bug 600098).
  if (IS_MACOSX) {
    let now = Date.now();
    let yesterday = now - (1000 * 60 * 60 * 24);
    let applyToDir = getApplyDirFile();
    applyToDir.lastModifiedTime = yesterday;
  }

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  runUpdateUsingService(STATE_PENDING_SVC, STATE_SUCCEEDED);
}

function checkUpdateFinished() {
  if (IS_MACOSX) {
    logTestInfo("testing last modified time on the apply to directory has " +
                "changed after a successful update (bug 600098)");
    let now = Date.now();
    let applyToDir = getApplyDirFile();
    let timeDiff = Math.abs(applyToDir.lastModifiedTime - now);
    do_check_true(timeDiff < MAC_MAX_TIME_DIFFERENCE);
  }

  checkFilesAfterUpdateSuccess();
  // Sorting on Linux is different so skip this check for now.
  if (!IS_UNIX) {
    checkUpdateLogContents(LOG_PARTIAL_SUCCESS);
  }

  checkCallbackServiceLog();
}
