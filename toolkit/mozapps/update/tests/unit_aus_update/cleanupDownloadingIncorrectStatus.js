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
  dir.append(DIR_PATCH);
  Assert.ok(dir.exists(), MSG_SHOULD_EXIST);

  let statusFile = dir.clone();
  statusFile.append(FILE_UPDATE_STATUS);
  Assert.ok(!statusFile.exists(), MSG_SHOULD_NOT_EXIST);

  Assert.ok(!gUpdateManager.activeUpdate,
            "there should not be an active update");
  Assert.equal(gUpdateManager.updateCount, 0,
               "the update manager update count" + MSG_SHOULD_EQUAL);

  doTestFinish();
}
