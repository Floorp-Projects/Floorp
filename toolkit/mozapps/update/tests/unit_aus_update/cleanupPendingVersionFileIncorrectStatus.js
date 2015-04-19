/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  setupTestCommon();

  debugDump("testing update cleanup when reading the status file returns " +
            "STATUS_NONE, the version file is for a newer version, and the " +
            "update xml has an update with STATE_PENDING (Bug 601701).");

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
  let patches = getLocalPatchString(null, null, null, null, null, null,
                                    STATE_PENDING);
  let updates = getLocalUpdateString(patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeVersionFile("99.9");

  standardInit();

  // Check that there is no activeUpdate first so the updates directory is
  // cleaned up by the UpdateManager before the remaining tests.
  Assert.ok(!gUpdateManager.activeUpdate,
            "there should not be an active update");
  Assert.equal(gUpdateManager.updateCount, 0,
               "the update manager update count" + MSG_SHOULD_EQUAL);

  let dir = getUpdatesDir();
  dir.append(DIR_PATCH);
  Assert.ok(dir.exists(), MSG_SHOULD_EXIST);

  let versionFile = dir.clone();
  versionFile.append(FILE_UPDATE_VERSION);
  Assert.ok(!versionFile.exists(), MSG_SHOULD_NOT_EXIST);

  doTestFinish();
}
