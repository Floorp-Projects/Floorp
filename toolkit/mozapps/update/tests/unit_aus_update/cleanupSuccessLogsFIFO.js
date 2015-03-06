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

  let dir = getUpdatesDir();
  let log = dir.clone();
  log.append(FILE_LAST_LOG);
  writeFile(log, "Backup Update Log");

  log = dir.clone();
  log.append(FILE_BACKUP_LOG);
  writeFile(log, "To Be Deleted Backup Update Log");

  log = dir.clone();
  log.append("0");
  log.append(FILE_UPDATE_LOG);
  writeFile(log, "Last Update Log");

  standardInit();

  debugDump("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  log = dir.clone();
  log.append(FILE_LAST_LOG);
  debugDump("testing " + log.path + " should exist");
  do_check_true(log.exists());

  debugDump("testing " + log.path + " contents");
  do_check_eq(readFile(log), "Last Update Log");

  log = dir.clone();
  log.append(FILE_BACKUP_LOG);
  debugDump("testing " + log.path + " should exist");
  do_check_true(log.exists());

  debugDump("testing " + log.path + " contents (bug 470979)");
  do_check_eq(readFile(log), "Backup Update Log");

  dir.append("0");
  debugDump("testing " + dir.path + " should exist (bug 512994)");
  do_check_true(dir.exists());

  doTestFinish();
}
