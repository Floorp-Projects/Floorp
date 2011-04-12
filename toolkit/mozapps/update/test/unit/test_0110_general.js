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

/* General Complete MAR File Patch Apply Test */

const TEST_ID = "0110";
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
  fileName         : "00text1",
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
  if (IS_ANDROID) {
    logTestInfo("this test is not applicable to Android... returning early");
    return;
  }

  do_test_pending();
  do_register_cleanup(cleanupUpdaterTest);

  setupUpdaterTest(MAR_COMPLETE_FILE);

  let updatesDir = do_get_file(TEST_ID + UPDATES_DIR_SUFFIX);
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

  logTestInfo("testing tobedeleted directory doesn't exist");
  let toBeDeletedDir = applyToDir.clone();
  toBeDeletedDir.append("tobedeleted");
  do_check_false(toBeDeletedDir.exists());

  checkCallbackAppLog();
}
