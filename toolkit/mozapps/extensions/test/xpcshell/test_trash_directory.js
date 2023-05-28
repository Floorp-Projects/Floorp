/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { BasePromiseWorker } = ChromeUtils.importESModule(
  "resource://gre/modules/PromiseWorker.sys.mjs"
);

// Test that an open file inside the trash directory does not cause
// unrelated installs to break (see bug 1180901 for more background).
add_task(async function test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  await promiseStartupManager();

  let profileDir = PathUtils.profileDir;
  let trashDir = PathUtils.join(profileDir, "extensions", "trash");
  let testFile = PathUtils.join(trashDir, "test.txt");

  await IOUtils.makeDirectory(trashDir);

  let trashDirExists = await IOUtils.exists(trashDir);
  ok(trashDirExists, "trash directory should have been created");

  await IOUtils.writeUTF8(testFile, "");

  // Use a worker to keep the testFile open.
  const worker = new BasePromiseWorker(
    "resource://test/data/test_trash_directory.worker.js"
  );
  await worker.post("open", [testFile]);

  let fileExists = await IOUtils.exists(testFile);
  ok(fileExists, "test.txt should have been created in " + trashDir);

  await promiseInstallWebExtension({});

  // The testFile should still exist at this point because we have not
  // yet closed the file handle and as a result, Windows cannot remove it.
  fileExists = await IOUtils.exists(testFile);
  ok(fileExists, "test.txt should still exist");

  // Cleanup
  await promiseShutdownManager();
  await worker.post("close", []);
  await IOUtils.remove(testFile);
  await IOUtils.remove(trashDir, { recursive: true });
});
