/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test an update isn't attempted when the update.status file can't be written
 * to.
 */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);

  // To simulate a user that doesn't have write access to the update directory
  // lock the relevant files in the update directory.
  let filesToLock = [
    FILE_ACTIVE_UPDATE_XML,
    FILE_UPDATE_MAR,
    FILE_UPDATE_STATUS,
    FILE_UPDATE_TEST,
    FILE_UPDATE_VERSION,
  ];
  filesToLock.forEach(function(aFileLeafName) {
    let file = getUpdateDirFile(aFileLeafName);
    if (!file.exists()) {
      file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o444);
    }
    file.QueryInterface(Ci.nsILocalFileWin);
    file.fileAttributesWin |= file.WFA_READONLY;
    file.fileAttributesWin &= ~file.WFA_READWRITE;
    Assert.ok(file.exists(), MSG_SHOULD_EXIST + getMsgPath(file.path));
    Assert.ok(!file.isWritable(), "the file should not be writeable");
  });

  registerCleanupFunction(() => {
    filesToLock.forEach(function(aFileLeafName) {
      let file = getUpdateDirFile(aFileLeafName);
      if (file.exists()) {
        file.QueryInterface(Ci.nsILocalFileWin);
        file.fileAttributesWin |= file.WFA_READWRITE;
        file.fileAttributesWin &= ~file.WFA_READONLY;
        file.remove(false);
      }
    });
  });

  // Reload the update manager now that the update directory files are locked.
  reloadUpdateManagerData();
  await runUpdateUsingApp(STATE_PENDING);
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateManager(STATE_PENDING, false, STATE_NONE, 0, 0);

  let dir = getUpdateDirFile(DIR_PATCH);
  Assert.ok(dir.exists(), MSG_SHOULD_EXIST + getMsgPath(dir.path));

  let file = getUpdateDirFile(FILE_UPDATES_XML);
  Assert.ok(!file.exists(), MSG_SHOULD_NOT_EXIST + getMsgPath(file.path));

  file = getUpdateDirFile(FILE_UPDATE_LOG);
  Assert.ok(!file.exists(), MSG_SHOULD_NOT_EXIST + getMsgPath(file.path));

  file = getUpdateDirFile(FILE_LAST_UPDATE_LOG);
  Assert.ok(!file.exists(), MSG_SHOULD_NOT_EXIST + getMsgPath(file.path));

  file = getUpdateDirFile(FILE_BACKUP_UPDATE_LOG);
  Assert.ok(!file.exists(), MSG_SHOULD_NOT_EXIST + getMsgPath(file.path));

  waitForFilesInUse();
}
