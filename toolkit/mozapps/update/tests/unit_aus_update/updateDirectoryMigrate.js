/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * Gets the root directory for the old (unmigrated) updates directory.
 *
 * @return nsIFile for the updates root directory.
 */
function getOldUpdatesRootDir() {
  return Services.dirsvc.get(XRE_OLD_UPDATE_ROOT_DIR, Ci.nsIFile);
}

/**
 * Gets the old (unmigrated) updates directory.
 *
 * @return nsIFile for the updates directory.
 */
function getOldUpdatesDir() {
  let dir = getOldUpdatesRootDir();
  dir.append(DIR_UPDATES);
  return dir;
}

/**
 * Gets the directory for update patches in the old (unmigrated) updates
 * directory.
 *
 * @return nsIFile for the updates directory.
 */
function getOldUpdatesPatchDir() {
  let dir = getOldUpdatesDir();
  dir.append(DIR_PATCH);
  return dir;
}

/**
 * Returns either the active or regular update database XML file in the old
 * (unmigrated) updates directory
 *
 * @param  isActiveUpdate
 *         If true this will return the active-update.xml otherwise it will
 *         return the updates.xml file.
 */
function getOldUpdatesXMLFile(aIsActiveUpdate) {
  let file = getOldUpdatesRootDir();
  file.append(aIsActiveUpdate ? FILE_ACTIVE_UPDATE_XML : FILE_UPDATES_XML);
  return file;
}

/**
 * Writes the updates specified to either the active-update.xml or the
 * updates.xml in the old (unmigrated) update directory
 *
 * @param  aContent
 *         The updates represented as a string to write to the XML file.
 * @param  isActiveUpdate
 *         If true this will write to the active-update.xml otherwise it will
 *         write to the updates.xml file.
 */
function writeUpdatesToOldXMLFile(aContent, aIsActiveUpdate) {
  writeFile(getOldUpdatesXMLFile(aIsActiveUpdate), aContent);
}

/**
 * Writes the given update operation/state to a file in the old (unmigrated)
 * patch directory, indicating to the patching system what operations need
 * to be performed.
 *
 * @param  aStatus
 *         The status value to write.
 */
function writeOldStatusFile(aStatus) {
  let file = getOldUpdatesPatchDir();
  file.append(FILE_UPDATE_STATUS);
  writeFile(file, aStatus + "\n");
}

/**
 * Writes the given data to the config file in the old (unmigrated)
 * patch directory.
 *
 * @param  aData
 *         The config data to write.
 */
function writeOldConfigFile(aData) {
  let file = getOldUpdatesRootDir();
  file.append(FILE_UPDATE_CONFIG_JSON);
  writeFile(file, aData);
}

/**
 * Gets the specified update log from the old (unmigrated) update directory
 *
 * @param   aLogLeafName
 *          The leaf name of the log to get.
 * @return  nsIFile for the update log.
 */
function getOldUpdateLog(aLogLeafName) {
  let updateLog = getOldUpdatesDir();
  if (aLogLeafName == FILE_UPDATE_LOG) {
    updateLog.append(DIR_PATCH);
  }
  updateLog.append(aLogLeafName);
  return updateLog;
}

async function run_test() {
  setupTestCommon(null);

  debugDump(
    "testing that the update directory is migrated after a successful update"
  );

  Services.prefs.setIntPref(PREF_APP_UPDATE_CANCELATIONS, 5);

  let patchProps = { state: STATE_PENDING };
  let patches = getLocalPatchString(patchProps);
  let updates = getLocalUpdateString({}, patches);
  writeUpdatesToOldXMLFile(getLocalUpdatesXMLString(updates), true);
  writeOldStatusFile(STATE_SUCCEEDED);
  writeOldConfigFile('{"app.update.auto":false}');

  let log = getOldUpdateLog(FILE_UPDATE_LOG);
  writeFile(log, "Last Update Log");

  let oldUninstallPingFile = getOldUpdatesRootDir();
  const hash = oldUninstallPingFile.leafName;
  const uninstallPingFilename = `uninstall_ping_${hash}_98537294-d37b-4b8b-a4e9-ab417a5d7a87.json`;
  oldUninstallPingFile = oldUninstallPingFile.parent.parent;
  oldUninstallPingFile.append(uninstallPingFilename);
  const uninstallPingContents = "arbitrary uninstall ping file contents";
  writeFile(oldUninstallPingFile, uninstallPingContents);

  let oldBackgroundUpdateLog1File = getOldUpdatesRootDir();
  const oldBackgroundUpdateLog1Filename = "backgroundupdate.moz_log";
  oldBackgroundUpdateLog1File.append(oldBackgroundUpdateLog1Filename);
  const oldBackgroundUpdateLog1Contents = "arbitrary log 1 contents";
  writeFile(oldBackgroundUpdateLog1File, oldBackgroundUpdateLog1Contents);

  let oldBackgroundUpdateLog2File = getOldUpdatesRootDir();
  const oldBackgroundUpdateLog2Filename = "backgroundupdate.child-1.moz_log";
  oldBackgroundUpdateLog2File.append(oldBackgroundUpdateLog2Filename);
  const oldBackgroundUpdateLog2Contents = "arbitrary log 2 contents";
  writeFile(oldBackgroundUpdateLog2File, oldBackgroundUpdateLog2Contents);

  const pendingPingRelativePath =
    "backgroundupdate\\datareporting\\glean\\pending_pings\\" +
    "01234567-89ab-cdef-fedc-0123456789ab";
  let oldPendingPingFile = getOldUpdatesRootDir();
  oldPendingPingFile.appendRelativePath(pendingPingRelativePath);
  const pendingPingContents = "arbitrary pending ping file contents";
  writeFile(oldPendingPingFile, pendingPingContents);

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
  await waitForUpdateXMLFiles();

  let cancelations = Services.prefs.getIntPref(PREF_APP_UPDATE_CANCELATIONS, 0);
  Assert.equal(
    cancelations,
    0,
    "the " + PREF_APP_UPDATE_CANCELATIONS + " preference " + MSG_SHOULD_EQUAL
  );

  let oldDir = getOldUpdatesRootDir();
  let newDir = getUpdateDirFile();
  if (oldDir.path != newDir.path) {
    Assert.ok(
      !oldDir.exists(),
      "Old update directory should have been deleted after migration"
    );
  }

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
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  let dir = getUpdateDirFile(DIR_PATCH);
  Assert.ok(dir.exists(), MSG_SHOULD_EXIST);

  Assert.equal(
    await UpdateUtils.getAppUpdateAutoEnabled(),
    false,
    "Automatic update download setting should have been migrated."
  );

  let newUninstallPing = newDir.parent.parent;
  newUninstallPing.append(uninstallPingFilename);
  Assert.ok(newUninstallPing.exists(), MSG_SHOULD_EXIST);
  Assert.equal(
    readFile(newUninstallPing),
    uninstallPingContents,
    "the uninstall ping contents" + MSG_SHOULD_EQUAL
  );

  let newBackgroundUpdateLog1File = newDir.clone();
  newBackgroundUpdateLog1File.append(oldBackgroundUpdateLog1Filename);
  Assert.ok(newBackgroundUpdateLog1File.exists(), MSG_SHOULD_EXIST);
  Assert.equal(
    readFile(newBackgroundUpdateLog1File),
    oldBackgroundUpdateLog1Contents,
    "background log file 1 contents" + MSG_SHOULD_EQUAL
  );

  let newBackgroundUpdateLog2File = newDir.clone();
  newBackgroundUpdateLog2File.append(oldBackgroundUpdateLog2Filename);
  Assert.ok(newBackgroundUpdateLog2File.exists(), MSG_SHOULD_EXIST);
  Assert.equal(
    readFile(newBackgroundUpdateLog2File),
    oldBackgroundUpdateLog2Contents,
    "background log file 2 contents" + MSG_SHOULD_EQUAL
  );

  let newPendingPing = newDir.clone();
  newPendingPing.appendRelativePath(pendingPingRelativePath);
  Assert.ok(newPendingPing.exists(), MSG_SHOULD_EXIST);
  Assert.equal(
    readFile(newPendingPing),
    pendingPingContents,
    "the pending ping contents" + MSG_SHOULD_EQUAL
  );

  doTestFinish();
}
