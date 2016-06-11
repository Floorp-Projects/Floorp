/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();

  debugDump("testing update logs are first in first out deleted");

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
  let patches = getLocalPatchString(null, null, null, null, null, null,
                                    STATE_PENDING);
  let updates = getLocalUpdateString(patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_SUCCEEDED);

  let log = getUpdateLog(FILE_LAST_UPDATE_LOG);
  writeFile(log, "Backup Update Log");

  log = getUpdateLog(FILE_BACKUP_UPDATE_LOG);
  writeFile(log, "To Be Deleted Backup Update Log");

  log = getUpdateLog(FILE_UPDATE_LOG);
  writeFile(log, "Last Update Log");

  standardInit();

  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  log = getUpdateLog(FILE_LAST_UPDATE_LOG);
  Assert.ok(log.exists(), MSG_SHOULD_EXIST);
  Assert.equal(readFile(log), "Last Update Log",
               "the last update log contents" + MSG_SHOULD_EQUAL);

  log = getUpdateLog(FILE_BACKUP_UPDATE_LOG);
  Assert.ok(log.exists(), MSG_SHOULD_EXIST);
  Assert.equal(readFile(log), "Backup Update Log",
               "the backup update log contents" + MSG_SHOULD_EQUAL);

  let dir = getUpdatesPatchDir();
  Assert.ok(dir.exists(), MSG_SHOULD_EXIST);

  doTestFinish();
}
