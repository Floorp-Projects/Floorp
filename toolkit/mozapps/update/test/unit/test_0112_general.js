/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Strong <robert.bugzilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

/* General Partial MAR File Patch Apply Failure Test */

const TEST_ID = "0112";

// The files are in the same order as they are applied from the mar
const TEST_FILES = [
{
  fileName         : "1_1_image1.png",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/1_1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/complete.png",
  compareFile      : "data/complete.png",
  originalPerms    : 0644,
  comparePerms     : null
}, {
  fileName         : "1_1_text1",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/1_1/",
  originalContents : "ShouldNotBeDeleted\n",
  compareContents  : "ShouldNotBeDeleted\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0644,
  comparePerms     : null
}, {
  fileName         : "1_1_text2",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/1_1/",
  originalContents : "ShouldNotBeDeleted\n",
  compareContents  : "ShouldNotBeDeleted\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0644,
  comparePerms     : null
}, {
  fileName         : "1_exe1.exe",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "data/partial.png",
  compareFile      : "data/partial.png",
  originalPerms    : 0755,
  comparePerms     : null
}, {
  fileName         : "2_1_text1",
  destinationDir   : TEST_ID + APPLY_TO_DIR_SUFFIX + "/mar_test/2/2_1/",
  originalContents : "ShouldNotBeDeleted\n",
  compareContents  : "ShouldNotBeDeleted\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0644,
  comparePerms     : null
}];

function run_test() {
  if (IS_ANDROID) {
    logTestInfo("this test is not applicable to Android... returning early");
    return;
  }

  do_test_pending();
  do_register_cleanup(end_test);

  setupUpdaterTest(TEST_ID, MAR_PARTIAL_FILE, TEST_FILES);

  // The testUpdate function is used for consistency with the tests that require
  // a timeout before continuing the test.
  testUpdate();
}

function end_test() {
  cleanupUpdaterTest(TEST_ID);
}

function testUpdate() {
  let updatesDir = do_get_file(TEST_ID + UPDATES_DIR_SUFFIX);
  let applyToDir = do_get_file(TEST_ID + APPLY_TO_DIR_SUFFIX);

  // For Mac OS X set the last modified time for a directory to a date in the
  // past to test that the last modified time on the directories in not updated
  // when an update fails (bug 600098).
  let lastModTime;
  if (IS_MACOSX) {
    // All we care about is that the last modified time has not changed when an
    // update has failed.
    let now = Date.now();
    lastModTime = now - (1000 * 60 * 60 * 24);
    applyToDir.lastModifiedTime = lastModTime;
    // Set lastModTime to the value the OS returns in case it is different than
    // the value stored by the OS.
    lastModTime = applyToDir.lastModifiedTime;
  }

  // apply the partial mar
  let exitValue = runUpdate(TEST_ID);
  logTestInfo("testing updater binary process exitValue for success when " +
              "applying a partial mar");
  do_check_eq(exitValue, 0);

  logTestInfo("testing update.status should be " + STATE_FAILED);
  // The update status format for a failure is failed: # where # is the error
  // code for the failure.
  do_check_eq(readStatusFile(updatesDir).split(": ")[0], STATE_FAILED);

  // For Mac OS X check that the last modified time for a directory has not been
  // updated after a failed update (bug 600098).
  if (IS_MACOSX) {
    logTestInfo("testing last modified time on the apply to directory has " +
                "not changed after a failed update (bug 600098)");
    do_check_eq(applyToDir.lastModifiedTime, lastModTime);
  }

  checkFilesAfterUpdateFailure(TEST_ID, TEST_FILES);

  logTestInfo("testing tobedeleted directory doesn't exist");
  let toBeDeletedDir = applyToDir.clone();
  toBeDeletedDir.append("tobedeleted");
  do_check_false(toBeDeletedDir.exists());

  checkCallbackAppLog(TEST_ID);
}
