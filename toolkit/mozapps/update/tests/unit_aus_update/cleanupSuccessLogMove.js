/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

async function run_test() {
  setupTestCommon();

  debugDump("testing that the update.log is moved after a successful update");

  Services.prefs.setIntPref(PREF_APP_UPDATE_CANCELATIONS, 5);

  let patchProps = { state: STATE_PENDING };
  let patches = getLocalPatchString(patchProps);
  let updates = getLocalUpdateString({}, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_SUCCEEDED);

  let log = getUpdateDirFile(FILE_UPDATE_LOG);
  writeFile(log, "Last Update Log");
  log = getUpdateDirFile(FILE_UPDATE_ELEVATED_LOG);
  writeFile(log, "Last Update Elevated Log");

  await standardInit();

  Assert.ok(
    !(await gUpdateManager.getDownloadingUpdate()),
    "there should not be a downloading update"
  );
  Assert.ok(
    !(await gUpdateManager.getReadyUpdate()),
    "there should not be a ready update"
  );
  const history = await gUpdateManager.getHistory();
  Assert.equal(
    history.length,
    1,
    "the update manager update count" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    gUpdateManager.updateInstalledAtStartup,
    history[0],
    "the update installed at startup should be the update from the history"
  );
  await waitForUpdateXMLFiles();

  let cancelations = Services.prefs.getIntPref(PREF_APP_UPDATE_CANCELATIONS, 0);
  Assert.equal(
    cancelations,
    0,
    "the " + PREF_APP_UPDATE_CANCELATIONS + " preference " + MSG_SHOULD_EQUAL
  );

  log = getUpdateDirFile(FILE_UPDATE_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  log = getUpdateDirFile(FILE_UPDATE_ELEVATED_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  log = getUpdateDirFile(FILE_LAST_UPDATE_LOG);
  Assert.ok(log.exists(), MSG_SHOULD_EXIST);
  Assert.equal(
    readFile(log),
    "Last Update Log",
    "the last update log contents" + MSG_SHOULD_EQUAL
  );

  log = getUpdateDirFile(FILE_LAST_UPDATE_ELEVATED_LOG);
  Assert.ok(log.exists(), MSG_SHOULD_EXIST);
  Assert.equal(
    readFile(log),
    "Last Update Elevated Log",
    "the last update elevated log contents" + MSG_SHOULD_EQUAL
  );

  log = getUpdateDirFile(FILE_BACKUP_UPDATE_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  log = getUpdateDirFile(FILE_BACKUP_UPDATE_ELEVATED_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  let dir = getUpdateDirFile(DIR_PATCH);
  Assert.ok(dir.exists(), MSG_SHOULD_EXIST);

  // Simulate the browser restarting by rerunning update initialization.
  reloadUpdateManagerData();
  await testPostUpdateProcessing();

  Assert.equal(
    gUpdateManager.updateInstalledAtStartup,
    null,
    "updateInstalledAtStartup should be cleared on next browser start"
  );

  doTestFinish();
}
