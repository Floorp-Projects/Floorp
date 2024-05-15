/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * Creates the specified log files and makes sure that they are rotated properly
 * when we successfully update and cleanup the old update files.
 *
 * We test various starting combinations of logs because we want to make sure
 * that, for example, when "update.log" exists but "update-elevated.log" does
 * not, that we still move "last-update-elevated.log" to
 * "backup-update-elevated.log". Allowing the files to be mismatched makes them
 * much less useful.
 *
 * When running this, either `createUpdateLog` or `createUpdateElevatedLog`
 * should be `true` because otherwise it doesn't make sense that an update just
 * ran successfully.
 *
 * @param createUpdateLog
 *        If `true`, "update.log" will be created at the start of the test.
 * @param createLastUpdateLog
 *        If `true`, "last-update.log" will be created at the start of the test.
 * @param createBackupUpdateLog
 *        If `true`, "backup-update.log" will be created at the start of the
 *        test.
 * @param createUpdateElevatedLog
 *        If `true`, "update-elevated.log" will be created at the start of the
 *        test.
 * @param createLastUpdateElevatedLog
 *        If `true`, "last-update-elevated.log" will be created at the start of
 *        the test.
 * @param createBackupUpdateElevatedLog
 *        If `true`, "backup-update-elevated.log" will be created at the start
 *        of the test.
 */
async function testCleanupSuccessLogsFIFO(
  createUpdateLog,
  createLastUpdateLog,
  createBackupUpdateLog,
  createUpdateElevatedLog,
  createLastUpdateElevatedLog,
  createBackupUpdateElevatedLog
) {
  logTestInfo(
    `createUpdateLog=${createUpdateLog} ` +
      `createLastUpdateLog=${createLastUpdateLog} ` +
      `createBackupUpdateLog=${createBackupUpdateLog} ` +
      `createUpdateElevatedLog=${createUpdateElevatedLog} ` +
      `createLastUpdateElevatedLog=${createLastUpdateElevatedLog} ` +
      `createBackupUpdateElevatedLog=${createBackupUpdateElevatedLog}`
  );

  let patchProps = { state: STATE_PENDING };
  let patches = getLocalPatchString(patchProps);
  let updates = getLocalUpdateString({}, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_SUCCEEDED);

  const createOrDeleteFile = (shouldCreate, filename, contents) => {
    let log = getUpdateDirFile(filename);
    if (shouldCreate) {
      writeFile(log, contents);
    } else {
      try {
        log.remove(false);
      } catch (ex) {
        if (ex.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
          throw ex;
        }
      }
    }
  };

  createOrDeleteFile(
    createLastUpdateLog,
    FILE_LAST_UPDATE_LOG,
    "Backup Update Log"
  );
  createOrDeleteFile(
    createBackupUpdateLog,
    FILE_BACKUP_UPDATE_LOG,
    "To Be Deleted Backup Update Log"
  );
  createOrDeleteFile(createUpdateLog, FILE_UPDATE_LOG, "Last Update Log");
  createOrDeleteFile(
    createLastUpdateElevatedLog,
    FILE_LAST_UPDATE_ELEVATED_LOG,
    "Backup Update Elevated Log"
  );
  createOrDeleteFile(
    createBackupUpdateElevatedLog,
    FILE_BACKUP_UPDATE_ELEVATED_LOG,
    "To Be Deleted Backup Update Elevated Log"
  );
  createOrDeleteFile(
    createUpdateElevatedLog,
    FILE_UPDATE_ELEVATED_LOG,
    "Last Update Elevated Log"
  );

  await testPostUpdateProcessing();

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

  let log = getUpdateDirFile(FILE_UPDATE_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  log = getUpdateDirFile(FILE_UPDATE_ELEVATED_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  log = getUpdateDirFile(FILE_LAST_UPDATE_LOG);
  if (createUpdateLog) {
    Assert.ok(log.exists(), MSG_SHOULD_EXIST);
    Assert.equal(
      readFile(log),
      "Last Update Log",
      "the last update log contents" + MSG_SHOULD_EQUAL
    );
  } else {
    Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);
  }

  log = getUpdateDirFile(FILE_LAST_UPDATE_ELEVATED_LOG);
  if (createUpdateElevatedLog) {
    Assert.ok(log.exists(), MSG_SHOULD_EXIST);
    Assert.equal(
      readFile(log),
      "Last Update Elevated Log",
      "the last update log contents" + MSG_SHOULD_EQUAL
    );
  } else {
    Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);
  }

  log = getUpdateDirFile(FILE_BACKUP_UPDATE_LOG);
  if (createLastUpdateLog) {
    Assert.ok(log.exists(), MSG_SHOULD_EXIST);
    Assert.equal(
      readFile(log),
      "Backup Update Log",
      "the backup update log contents" + MSG_SHOULD_EQUAL
    );
  } else if (!createLastUpdateElevatedLog && createBackupUpdateLog) {
    // This isn't really a conventional FIFO. We don't shift the old backup logs
    // out if, for some reason the last pair of logs didn't exist.
    Assert.ok(log.exists(), MSG_SHOULD_EXIST);
    Assert.equal(
      readFile(log),
      "To Be Deleted Backup Update Log",
      "the backup update log contents" + MSG_SHOULD_EQUAL
    );
  } else {
    Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);
  }

  log = getUpdateDirFile(FILE_BACKUP_UPDATE_ELEVATED_LOG);
  if (createLastUpdateElevatedLog) {
    Assert.ok(log.exists(), MSG_SHOULD_EXIST);
    Assert.equal(
      readFile(log),
      "Backup Update Elevated Log",
      "the backup update log contents" + MSG_SHOULD_EQUAL
    );
  } else if (!createLastUpdateLog && createBackupUpdateElevatedLog) {
    // This isn't really a conventional FIFO. We don't shift the old backup logs
    // out if, for some reason the last pair of logs didn't exist.
    Assert.ok(log.exists(), MSG_SHOULD_EXIST);
    Assert.equal(
      readFile(log),
      "To Be Deleted Backup Update Elevated Log",
      "the backup update log contents" + MSG_SHOULD_EQUAL
    );
  } else {
    Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);
  }

  let dir = getUpdateDirFile(DIR_PATCH);
  Assert.ok(dir.exists(), MSG_SHOULD_EXIST);

  // Clean up so this function can run again.
  reloadUpdateManagerData(true);
}

async function run_test() {
  debugDump("testing update logs are first in first out deleted");
  setupTestCommon();

  // This runs a bunch of tests (I think 48 of them: 2^6 - 2^4), but each test
  // is fairly simple and doesn't do a lot of waiting. So it seems unlikely that
  // it will exceed its timeout.
  for (const createUpdateLog of [true, false]) {
    for (const createUpdateElevatedLog of [true, false]) {
      if (!createUpdateLog && !createUpdateElevatedLog) {
        continue;
      }
      for (const createLastUpdateLog of [true, false]) {
        for (const createLastUpdateElevatedLog of [true, false]) {
          for (const createBackupUpdateLog of [true, false]) {
            for (const createBackupUpdateElevatedLog of [true, false]) {
              await testCleanupSuccessLogsFIFO(
                createUpdateLog,
                createLastUpdateLog,
                createBackupUpdateLog,
                createUpdateElevatedLog,
                createLastUpdateElevatedLog,
                createBackupUpdateElevatedLog
              );
            }
          }
        }
      }
    }
  }
  doTestFinish();
}
