/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that an open file inside the trash directory does not cause
// unrelated installs to break (see bug 1180901 for more background).
add_task(async function test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  await promiseStartupManager();

  let profileDir = OS.Constants.Path.profileDir;
  let trashDir = OS.Path.join(profileDir, "extensions", "trash");
  let testFile = OS.Path.join(trashDir, "test.txt");

  await OS.File.makeDir(trashDir, {
    from: profileDir,
    ignoreExisting: true,
  });

  let trashDirExists = await OS.File.exists(trashDir);
  ok(trashDirExists, "trash directory should have been created");

  let file = await OS.File.open(testFile, { create: true }, { winShare: 0 });
  let fileExists = await OS.File.exists(testFile);
  ok(fileExists, "test.txt should have been created in " + trashDir);

  await promiseInstallWebExtension({});

  // The testFile should still exist at this point because we have not
  // yet closed the file handle and as a result, Windows cannot remove it.
  fileExists = await OS.File.exists(testFile);
  ok(fileExists, "test.txt should still exist");

  // Cleanup
  await promiseShutdownManager();
  await file.close();
  await OS.File.remove(testFile);
  await OS.File.removeDir(trashDir);
});
