/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Since PlacesBackups.getbackupFiles() is a lazy getter, these tests must
// run in the given order, to avoid making it out-of-sync.

async function countChildren(path) {
  let children = await IOUtils.getChildren(path);
  let count = 0;
  let lastBackupPath = null;
  for (let entry of children) {
    count++;
    if (PlacesBackups.filenamesRegex.test(PathUtils.filename(entry))) {
      lastBackupPath = entry;
    }
  }
  return { count, lastBackupPath };
}

add_task(async function check_max_backups_is_respected() {
  // Get bookmarkBackups directory
  let backupFolder = await PlacesBackups.getBackupFolder();

  // Create 2 json dummy backups in the past.
  let oldJsonPath = PathUtils.join(backupFolder, "bookmarks-2008-01-01.json");
  await IOUtils.writeUTF8(oldJsonPath, "");
  Assert.ok(await IOUtils.exists(oldJsonPath));

  let jsonPath = PathUtils.join(backupFolder, "bookmarks-2008-01-31.json");
  await IOUtils.writeUTF8(jsonPath, "");
  Assert.ok(await IOUtils.exists(jsonPath));

  // Export bookmarks to JSON.
  // Allow 2 backups, the older one should be removed.
  await PlacesBackups.create(2);

  let { count, lastBackupPath } = await countChildren(backupFolder);
  Assert.equal(count, 2);
  Assert.notEqual(lastBackupPath, null);
  Assert.equal(false, await IOUtils.exists(oldJsonPath));
  Assert.ok(await IOUtils.exists(jsonPath));
});

add_task(async function check_max_backups_greater_than_backups() {
  // Get bookmarkBackups directory
  let backupFolder = await PlacesBackups.getBackupFolder();

  // Export bookmarks to JSON.
  // Allow 3 backups, none should be removed.
  await PlacesBackups.create(3);

  let { count, lastBackupPath } = await countChildren(backupFolder);
  Assert.equal(count, 2);
  Assert.notEqual(lastBackupPath, null);
});

add_task(async function check_max_backups_null() {
  // Get bookmarkBackups directory
  let backupFolder = await PlacesBackups.getBackupFolder();

  // Export bookmarks to JSON.
  // Allow infinite backups, none should be removed, a new one is not created
  // since one for today already exists.
  await PlacesBackups.create(null);

  let { count, lastBackupPath } = await countChildren(backupFolder);
  Assert.equal(count, 2);
  Assert.notEqual(lastBackupPath, null);
});

add_task(async function check_max_backups_undefined() {
  // Get bookmarkBackups directory
  let backupFolder = await PlacesBackups.getBackupFolder();

  // Export bookmarks to JSON.
  // Allow infinite backups, none should be removed, a new one is not created
  // since one for today already exists.
  await PlacesBackups.create();

  let { count, lastBackupPath } = await countChildren(backupFolder);
  Assert.equal(count, 2);
  Assert.notEqual(lastBackupPath, null);
});
