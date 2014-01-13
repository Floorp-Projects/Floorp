/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  setupTestCommon();

  logTestInfo("testing update cleanup when reading the status file returns " +
              "STATUS_NONE and the update xml has an update with " +
              "STATE_DOWNLOADING (Bug 539717).");

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

  logTestInfo("testing activeUpdate == null");
  do_check_eq(gUpdateManager.activeUpdate, null);
  logTestInfo("testing updateCount == 0");
  do_check_eq(gUpdateManager.updateCount, 0);

  doTestFinish();
}
