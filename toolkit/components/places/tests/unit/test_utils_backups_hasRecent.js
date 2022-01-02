/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Check for correct functionality of bookmarks backups
 */

/**
 * Creates a fake empty dated backup.
 * @param {Date} date the Date to use for the backup file name
 * @param {string} backupsFolderPath the path to the backups folder
 * @returns path of the created backup file
 */
async function createFakeBackup(date, backupsFolderPath) {
  let backupFilePath = PathUtils.join(
    backupsFolderPath,
    PlacesBackups.getFilenameForDate(date)
  );
  await IOUtils.write(backupFilePath, new Uint8Array());
  return backupFilePath;
}

add_task(async function test_hasRecentBackup() {
  let backupFolderPath = await PlacesBackups.getBackupFolder();
  Assert.ok(!(await PlacesBackups.hasRecentBackup()), "Check no recent backup");

  await createFakeBackup(new Date(Date.now() - 4 * 86400), backupFolderPath);
  Assert.ok(!(await PlacesBackups.hasRecentBackup()), "Check no recent backup");
  PlacesBackups.invalidateCache();
  await createFakeBackup(new Date(Date.now() - 2 * 86400), backupFolderPath);
  Assert.ok(await PlacesBackups.hasRecentBackup(), "Check has recent backup");
  PlacesBackups.invalidateCache();

  try {
    await IOUtils.remove(backupFolderPath, { recursive: true });
  } catch (ex) {
    // On Windows the files may be locked.
    info("Unable to cleanup the backups test folder");
  }
});
