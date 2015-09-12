/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();
  run_next_test();
}

add_task(function* () {
  let profileDir = OS.Constants.Path.profileDir;
  let trashDir = OS.Path.join(profileDir, "extensions", "trash");
  let testFile = OS.Path.join(trashDir, "test.txt");

  yield OS.File.makeDir(trashDir, {
    from: profileDir,
    ignoreExisting: true
  });

  let trashDirExists = yield OS.File.exists(trashDir);
  ok(trashDirExists, "trash directory should have been created");

  let file = yield OS.File.open(testFile, {create: true}, {winShare: 0});
  let fileExists = yield OS.File.exists(testFile);
  ok(fileExists, "test.txt should have been created in " + trashDir);

  let promiseInstallStatus = new Promise((resolve, reject) => {
    let listener = {
      onInstallFailed: function() {
        AddonManager.removeInstallListener(listener);
        reject("extension installation should not have failed");
      },
      onInstallEnded: function() {
        AddonManager.removeInstallListener(listener);
        ok(true, "extension installation should not have failed");
        resolve();
      }
    };

    AddonManager.addInstallListener(listener);
  });

  yield promiseInstallAllFiles([do_get_addon("test_bootstrap1_1")]);

  // The testFile should still exist at this point because we have not
  // yet closed the file handle and as a result, Windows cannot remove it.
  fileExists = yield OS.File.exists(testFile);
  ok(fileExists, "test.txt should still exist");

  // Wait for the AddonManager to tell us if the installation of the extension
  // succeeded or not.
  yield promiseInstallStatus;

  // Cleanup
  yield promiseShutdownManager();
  yield file.close();
  yield OS.File.remove(testFile);
  yield OS.File.removeDir(trashDir);
});
