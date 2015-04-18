/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();

  debugDump("testing that the update.log is moved after a successful update");

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
  let patches = getLocalPatchString(null, null, null, null, null, null,
                                    STATE_PENDING);
  let updates = getLocalUpdateString(patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_SUCCEEDED);

  let dir = getUpdatesDir();
  let log = dir.clone();
  log.append(DIR_PATCH);
  log.append(FILE_UPDATE_LOG);
  writeFile(log, "Last Update Log");

  standardInit();

  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  log = dir.clone();
  log.append(FILE_LAST_LOG);
  Assert.ok(log.exists(), MSG_SHOULD_EXIST);
  Assert.equal(readFile(log), "Last Update Log",
               "the last update log contents" + MSG_SHOULD_EQUAL);

  log = dir.clone();
  log.append(FILE_BACKUP_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  dir.append(DIR_PATCH);
  Assert.ok(dir.exists(), MSG_SHOULD_EXIST);

  doTestFinish();
}
