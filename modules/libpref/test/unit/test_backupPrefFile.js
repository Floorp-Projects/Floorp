/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that we can create a backup of the preferences state to
 * a file asynchronously.
 */
add_task(async function test_backupPrefFile() {
  // Create a backup of the preferences state to a file.
  Services.prefs.setBoolPref("test.backup", true);

  let backupFilePath = PathUtils.join(PathUtils.tempDir, "prefs-backup.js");
  let backupFile = await IOUtils.getFile(backupFilePath);
  await Services.prefs.backupPrefFile(backupFile);

  // Verify that the backup file was created and contains the expected content.
  let backupContent = await IOUtils.read(backupFilePath, { encoding: "utf-8" });

  let sawTestValue = false;

  // Now parse the backup file and verify that it contains the expected
  // preference value. We'll not worry about any of the other preferences.
  let observer = {
    onStringPref() {},
    onIntPref() {},
    onBoolPref(kind, name, value, _isSticky, _isLocked) {
      if (name == "test.backup" && value) {
        sawTestValue = true;
      }
    },
    onError(message) {
      Assert.ok(false, "Error while parsing backup file: " + message);
    },
  };
  Services.prefs.parsePrefsFromBuffer(backupContent, observer);

  Assert.ok(
    sawTestValue,
    "The backup file contains the expected preference value."
  );

  // Clean up the backup file.
  await IOUtils.remove(backupFilePath);
});
