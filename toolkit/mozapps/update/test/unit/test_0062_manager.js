/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Update Manager Tests */

const TEST_ID = "0062";

function run_test() {
  do_test_pending();

  // adjustGeneralPaths registers a cleanup function that calls end_test.
  adjustGeneralPaths();

  logTestInfo("testing resuming an update download in progress for the same " +
              "version of the application on startup (bug 485624)");
  removeUpdateDirsAndFiles();

  var patches, updates;

  patches = getLocalPatchString(null, null, null, null, null, null,
                                STATE_DOWNLOADING);
  updates = getLocalUpdateString(patches, null, null, "1.0", "1.0");
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_DOWNLOADING);

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);

  standardInit();

  if (IS_TOOLKIT_GONK) {
    // GONK doesn't resume downloads at boot time, so the updateCount will
    // always be zero.
    do_check_eq(gUpdateManager.updateCount, 0);
  } else {
    do_check_eq(gUpdateManager.updateCount, 1);
  }
  do_check_eq(gUpdateManager.activeUpdate.state, STATE_DOWNLOADING);

  do_test_finished();
}

function end_test() {
  cleanUp();
}
