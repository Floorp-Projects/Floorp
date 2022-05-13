/* Bug 1016953 - When a previous bookmark backup exists with the same hash
regardless of date, an automatic backup should attempt to either rename it to
today's date if the backup was for an old date or leave it alone if it was for
the same date. However if the file ext was json it will accidentally rename it
to jsonlz4 while keeping the json contents
*/

add_task(async function test_same_date_same_hash() {
  // If old file has been created on the same date and has the same hash
  // the file should be left alone
  let backupFolder = await PlacesBackups.getBackupFolder();
  // Save to profile dir to obtain hash and nodeCount to append to filename
  let tempPath = PathUtils.join(
    PathUtils.profileDir,
    "bug10169583_bookmarks.json"
  );
  let { count, hash } = await BookmarkJSONUtils.exportToFile(tempPath);

  // Save JSON file in backup folder with hash appended
  let dateObj = new Date();
  let filename =
    "bookmarks-" +
    PlacesBackups.toISODateString(dateObj) +
    "_" +
    count +
    "_" +
    hash +
    ".json";
  let backupFile = PathUtils.join(backupFolder, filename);
  await IOUtils.move(tempPath, backupFile);

  // Force a compressed backup which fallbacks to rename
  await PlacesBackups.create();
  let mostRecentBackupFile = await PlacesBackups.getMostRecentBackup();
  // check to ensure not renamed to jsonlz4
  Assert.equal(mostRecentBackupFile, backupFile);
  // inspect contents and check if valid json
  info("Check is valid JSON");
  // We initially wrote an uncompressed file, and although a backup was triggered
  // it did not rewrite the file, so this is uncompressed.
  await IOUtils.readJSON(mostRecentBackupFile);

  // Cleanup
  await IOUtils.remove(backupFile);
  await IOUtils.remove(tempPath);
  PlacesBackups._backupFiles = null; // To force re-cache of backupFiles
});

add_task(async function test_same_date_diff_hash() {
  // If the old file has been created on the same date, but has a different hash
  // the existing file should be overwritten with the newer compressed version
  let backupFolder = await PlacesBackups.getBackupFolder();
  let tempPath = PathUtils.join(
    PathUtils.profileDir,
    "bug10169583_bookmarks.json"
  );
  let { count } = await BookmarkJSONUtils.exportToFile(tempPath);
  let dateObj = new Date();
  let filename =
    "bookmarks-" +
    PlacesBackups.toISODateString(dateObj) +
    "_" +
    count +
    "_differentHash==.json";
  let backupFile = PathUtils.join(backupFolder, filename);
  await IOUtils.move(tempPath, backupFile);
  await PlacesBackups.create(); // Force compressed backup
  let mostRecentBackupFile = await PlacesBackups.getMostRecentBackup();

  // Decode lz4 compressed file to json and check if json is valid
  info("Check is valid JSON");
  await IOUtils.readJSON(mostRecentBackupFile, { decompress: true });

  // Cleanup
  await IOUtils.remove(mostRecentBackupFile);
  await IOUtils.remove(tempPath);
  PlacesBackups._backupFiles = null; // To force re-cache of backupFiles
});

add_task(async function test_diff_date_same_hash() {
  // If the old file has been created on an older day but has the same hash
  // it should be renamed with today's date without altering the contents.
  let backupFolder = await PlacesBackups.getBackupFolder();
  let tempPath = PathUtils.join(
    PathUtils.profileDir,
    "bug10169583_bookmarks.json"
  );
  let { count, hash } = await BookmarkJSONUtils.exportToFile(tempPath);
  let oldDate = new Date(2014, 1, 1);
  let curDate = new Date();
  let oldFilename =
    "bookmarks-" +
    PlacesBackups.toISODateString(oldDate) +
    "_" +
    count +
    "_" +
    hash +
    ".json";
  let newFilename =
    "bookmarks-" +
    PlacesBackups.toISODateString(curDate) +
    "_" +
    count +
    "_" +
    hash +
    ".json";
  let backupFile = PathUtils.join(backupFolder, oldFilename);
  let newBackupFile = PathUtils.join(backupFolder, newFilename);
  await IOUtils.move(tempPath, backupFile);

  // Ensure file has been renamed correctly
  await PlacesBackups.create();
  let mostRecentBackupFile = await PlacesBackups.getMostRecentBackup();
  Assert.equal(mostRecentBackupFile, newBackupFile);

  // Cleanup
  await IOUtils.remove(mostRecentBackupFile);
  await IOUtils.remove(tempPath);
});
