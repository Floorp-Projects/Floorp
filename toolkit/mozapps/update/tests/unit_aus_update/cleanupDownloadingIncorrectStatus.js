/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  setupTestCommon();

  debugDump("testing update cleanup when reading the status file returns " +
            "STATUS_NONE and the update xml has an update with " +
            "STATE_DOWNLOADING (Bug 539717).");

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
  let patches = getLocalPatchString(null, null, null, null, null, null,
                                    STATE_DOWNLOADING);
  let updates = getLocalUpdateString(patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_NONE);

  standardInit();

  let dir = getUpdatesDir();
  dir.append("0");
  debugDump("testing " + dir.path + " should exist");
  do_check_true(dir.exists());

  let statusFile = dir.clone();
  statusFile.append(FILE_UPDATE_STATUS);
  debugDump("testing " + statusFile.path + " should not exist");
  do_check_false(statusFile.exists());

  debugDump("testing activeUpdate == null");
  do_check_eq(gUpdateManager.activeUpdate, null);
  debugDump("testing updateCount == 0");
  do_check_eq(gUpdateManager.updateCount, 0);

  doTestFinish();
}
