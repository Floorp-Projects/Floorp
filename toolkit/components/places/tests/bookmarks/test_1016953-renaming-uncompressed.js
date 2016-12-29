function run_test() {
  run_next_test();
}

/* Bug 1016953 - When a previous bookmark backup exists with the same hash
regardless of date, an automatic backup should attempt to either rename it to
today's date if the backup was for an old date or leave it alone if it was for
the same date. However if the file ext was json it will accidentally rename it
to jsonlz4 while keeping the json contents
*/

add_task(function* test_same_date_same_hash() {
  // If old file has been created on the same date and has the same hash
  // the file should be left alone
  let backupFolder = yield PlacesBackups.getBackupFolder();
  // Save to profile dir to obtain hash and nodeCount to append to filename
  let tempPath = OS.Path.join(OS.Constants.Path.profileDir,
                              "bug10169583_bookmarks.json");
  let {count, hash} = yield BookmarkJSONUtils.exportToFile(tempPath);

  // Save JSON file in backup folder with hash appended
  let dateObj = new Date();
  let filename = "bookmarks-" + PlacesBackups.toISODateString(dateObj) + "_" +
                  count + "_" + hash + ".json";
  let backupFile = OS.Path.join(backupFolder, filename);
  yield OS.File.move(tempPath, backupFile);

  // Force a compressed backup which fallbacks to rename
  yield PlacesBackups.create();
  let mostRecentBackupFile = yield PlacesBackups.getMostRecentBackup();
  // check to ensure not renamed to jsonlz4
  Assert.equal(mostRecentBackupFile, backupFile);
  // inspect contents and check if valid json
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                        createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  let result = yield OS.File.read(mostRecentBackupFile);
  let jsonString = converter.convertFromByteArray(result, result.length);
  do_print("Check is valid JSON");
  JSON.parse(jsonString);

  // Cleanup
  yield OS.File.remove(backupFile);
  yield OS.File.remove(tempPath);
  PlacesBackups._backupFiles = null; // To force re-cache of backupFiles
});

add_task(function* test_same_date_diff_hash() {
  // If the old file has been created on the same date, but has a different hash
  // the existing file should be overwritten with the newer compressed version
  let backupFolder = yield PlacesBackups.getBackupFolder();
  let tempPath = OS.Path.join(OS.Constants.Path.profileDir,
                              "bug10169583_bookmarks.json");
  let {count} = yield BookmarkJSONUtils.exportToFile(tempPath);
  let dateObj = new Date();
  let filename = "bookmarks-" + PlacesBackups.toISODateString(dateObj) + "_" +
                  count + "_" + "differentHash==" + ".json";
  let backupFile = OS.Path.join(backupFolder, filename);
  yield OS.File.move(tempPath, backupFile);
  yield PlacesBackups.create(); // Force compressed backup
  let mostRecentBackupFile = yield PlacesBackups.getMostRecentBackup();

  // Decode lz4 compressed file to json and check if json is valid
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                        createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  let result = yield OS.File.read(mostRecentBackupFile, { compression: "lz4" });
  let jsonString = converter.convertFromByteArray(result, result.length);
  do_print("Check is valid JSON");
  JSON.parse(jsonString);

  // Cleanup
  yield OS.File.remove(mostRecentBackupFile);
  yield OS.File.remove(tempPath);
  PlacesBackups._backupFiles = null; // To force re-cache of backupFiles
});

add_task(function* test_diff_date_same_hash() {
  // If the old file has been created on an older day but has the same hash
  // it should be renamed with today's date without altering the contents.
  let backupFolder = yield PlacesBackups.getBackupFolder();
  let tempPath = OS.Path.join(OS.Constants.Path.profileDir,
                              "bug10169583_bookmarks.json");
  let {count, hash} = yield BookmarkJSONUtils.exportToFile(tempPath);
  let oldDate = new Date(2014, 1, 1);
  let curDate = new Date();
  let oldFilename = "bookmarks-" + PlacesBackups.toISODateString(oldDate) + "_" +
                  count + "_" + hash + ".json";
  let newFilename = "bookmarks-" + PlacesBackups.toISODateString(curDate) + "_" +
                  count + "_" + hash + ".json";
  let backupFile = OS.Path.join(backupFolder, oldFilename);
  let newBackupFile = OS.Path.join(backupFolder, newFilename);
  yield OS.File.move(tempPath, backupFile);

  // Ensure file has been renamed correctly
  yield PlacesBackups.create();
  let mostRecentBackupFile = yield PlacesBackups.getMostRecentBackup();
  Assert.equal(mostRecentBackupFile, newBackupFile);

  // Cleanup
  yield OS.File.remove(mostRecentBackupFile);
  yield OS.File.remove(tempPath);
});
