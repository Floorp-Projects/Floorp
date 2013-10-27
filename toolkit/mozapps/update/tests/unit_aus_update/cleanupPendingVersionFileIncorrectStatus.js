/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  setupTestCommon(true);

  logTestInfo("testing update cleanup when reading the status file returns " +
              "STATUS_NONE, the version file is for a newer version, and the " +
              "update xml has an update with STATE_PENDING (Bug 601701).");

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
  var patches = getLocalPatchString(null, null, null, null, null, null,
                                    STATE_PENDING);
  var updates = getLocalUpdateString(patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeVersionFile("99.9");

  standardInit();

  // Check that there is no activeUpdate first so the updates directory is
  // cleaned up by the UpdateManager before the remaining tests.
  logTestInfo("testing activeUpdate == null");
  do_check_eq(gUpdateManager.activeUpdate, null);
  logTestInfo("testing updateCount == 0");
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
  cleanupTestCommon();
}
