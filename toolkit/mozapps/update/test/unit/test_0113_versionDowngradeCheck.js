/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Test version downgrade MAR security check */

const TEST_ID = "0113";

// We don't actually care if the MAR has any data, we only care about the
// application return code and update.status result.
const TEST_FILES = [];

const VERSION_DOWNGRADE_ERROR = "23";

function run_test() {
  if (!IS_MAR_CHECKS_ENABLED) {
    return;
  }

  // Setup an old version MAR file
  do_register_cleanup(cleanupUpdaterTest);
  setupUpdaterTest(MAR_OLD_VERSION_FILE);

  // Apply the MAR
  let exitValue = runUpdate();
  logTestInfo("testing updater binary process exitValue for failure when " +
              "applying a version downgrade MAR");
  // Make sure the updater execution failed.
  // Note that if execv is used, the updater process will turn into the
  // callback process, so its return code will be that of the callback
  // app.
  do_check_eq(exitValue, USE_EXECV ? 0 : 1);
  let updatesDir = do_get_file(TEST_ID + UPDATES_DIR_SUFFIX);

  //Make sure we get a version downgrade error
  let updateStatus = readStatusFile(updatesDir);
  do_check_eq(updateStatus.split(": ")[1], VERSION_DOWNGRADE_ERROR);
}
