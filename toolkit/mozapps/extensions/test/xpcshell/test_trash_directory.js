/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ADDONS = {
  test_bootstrap1_1: {
    "install.rdf": {
      "id": "bootstrap1@tests.mozilla.org",
      "name": "Test Bootstrap 1",
    },
    "bootstrap.js": BOOTSTRAP_MONITOR_BOOTSTRAP_JS
  },
};

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  await promiseStartupManager();
});

add_task(async function() {
  let profileDir = OS.Constants.Path.profileDir;
  let trashDir = OS.Path.join(profileDir, "extensions", "trash");
  let testFile = OS.Path.join(trashDir, "test.txt");

  await OS.File.makeDir(trashDir, {
    from: profileDir,
    ignoreExisting: true
  });

  let trashDirExists = await OS.File.exists(trashDir);
  ok(trashDirExists, "trash directory should have been created");

  let file = await OS.File.open(testFile, {create: true}, {winShare: 0});
  let fileExists = await OS.File.exists(testFile);
  ok(fileExists, "test.txt should have been created in " + trashDir);

  let promiseInstallStatus = new Promise((resolve, reject) => {
    let listener = {
      onInstallFailed() {
        AddonManager.removeInstallListener(listener);
        reject("extension installation should not have failed");
      },
      onInstallEnded() {
        AddonManager.removeInstallListener(listener);
        ok(true, "extension installation should not have failed");
        resolve();
      }
    };

    AddonManager.addInstallListener(listener);
  });

  await AddonTestUtils.promiseInstallXPI(ADDONS.test_bootstrap1_1);

  // The testFile should still exist at this point because we have not
  // yet closed the file handle and as a result, Windows cannot remove it.
  fileExists = await OS.File.exists(testFile);
  ok(fileExists, "test.txt should still exist");

  // Wait for the AddonManager to tell us if the installation of the extension
  // succeeded or not.
  await promiseInstallStatus;

  // Cleanup
  await promiseShutdownManager();
  await file.close();
  await OS.File.remove(testFile);
  await OS.File.removeDir(trashDir);
});
