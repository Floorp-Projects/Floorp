/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* General Update Directory Cleanup Tests - Bug 601701 */

const TEST_ID = "0073";

function run_test() {
  do_test_pending();

  // adjustGeneralPaths registers a cleanup function that calls end_test.
  adjustGeneralPaths();

  removeUpdateDirsAndFiles();

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
  var patches = getLocalPatchString(null, null, null, null, null, null,
                                    STATE_PENDING);
  var updates = getLocalUpdateString(patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeVersionFile("99.9");

  standardInit();

  // Check that there is no activeUpdate first so the updates directory is
  // cleaned before the remaining tests.
  do_check_eq(gUpdateManager.activeUpdate, null);
  do_check_eq(gUpdateManager.updateCount, 0);

  var dir = getUpdatesDir();
  dir.append("0");
  logTestInfo("testing " + dir.path + " should exist");
  do_check_true(dir.exists());

  var versionFile = dir.clone();
  versionFile.append(FILE_UPDATE_VERSION);
  logTestInfo("testing " + versionFile.path + " should not exist");
  do_check_false(versionFile.exists());

  do_test_finished();
}

function end_test() {
  cleanUp();
}
