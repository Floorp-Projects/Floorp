/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* General Update Directory Cleanup Tests - Bug 539717 */

const TEST_ID = "0072";

function run_test() {
  do_test_pending();

  // adjustGeneralPaths registers a cleanup function that calls end_test.
  adjustGeneralPaths();

  removeUpdateDirsAndFiles();

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
  var patches = getLocalPatchString(null, null, null, null, null, null,
                                    STATE_DOWNLOADING);
  var updates = getLocalUpdateString(patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_NONE);

  standardInit();

  var dir = getUpdatesDir();
  dir.append("0");
  logTestInfo("testing " + dir.path + " should exist");
  do_check_true(dir.exists());

  var statusFile = dir.clone();
  statusFile.append(FILE_UPDATE_STATUS);
  logTestInfo("testing " + statusFile.path + " should not exist");
  do_check_false(statusFile.exists());

  do_check_eq(gUpdateManager.activeUpdate, null);
  do_check_eq(gUpdateManager.updateCount, 0);

  do_test_finished();
}

function end_test() {
  cleanUp();
}
