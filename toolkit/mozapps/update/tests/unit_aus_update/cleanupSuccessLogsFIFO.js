/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

async function run_test() {
  setupTestCommon();

  debugDump("testing update logs are first in first out deleted");

  let patchProps = { state: STATE_PENDING };
  let patches = getLocalPatchString(patchProps);
  let updates = getLocalUpdateString({}, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_SUCCEEDED);

  let log = getUpdateDirFile(FILE_LAST_UPDATE_LOG);
  writeFile(log, "Backup Update Log");

  log = getUpdateDirFile(FILE_BACKUP_UPDATE_LOG);
  writeFile(log, "To Be Deleted Backup Update Log");

  log = getUpdateDirFile(FILE_UPDATE_LOG);
  writeFile(log, "Last Update Log");

  standardInit();

  Assert.ok(
    !gUpdateManager.downloadingUpdate,
    "there should not be a downloading update"
  );
  Assert.ok(!gUpdateManager.readyUpdate, "there should not be a ready update");
  Assert.equal(
    gUpdateManager.getUpdateCount(),
    1,
    "the update manager update count" + MSG_SHOULD_EQUAL
  );
  await waitForUpdateXMLFiles();

  log = getUpdateDirFile(FILE_UPDATE_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  log = getUpdateDirFile(FILE_LAST_UPDATE_LOG);
  Assert.ok(log.exists(), MSG_SHOULD_EXIST);
  Assert.equal(
    readFile(log),
    "Last Update Log",
    "the last update log contents" + MSG_SHOULD_EQUAL
  );

  log = getUpdateDirFile(FILE_BACKUP_UPDATE_LOG);
  Assert.ok(log.exists(), MSG_SHOULD_EXIST);
  Assert.equal(
    readFile(log),
    "Backup Update Log",
    "the backup update log contents" + MSG_SHOULD_EQUAL
  );

  let dir = getUpdateDirFile(DIR_PATCH);
  Assert.ok(dir.exists(), MSG_SHOULD_EXIST);

  doTestFinish();
}
