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

  yield promiseInstallAllFiles([do_get_addon("test_install1")]);
  yield promiseRestartManager();
  fileExists = yield OS.File.exists(testFile);
  ok(fileExists, "test.txt still exists");
  yield file.close();
  yield OS.File.removeDir(OS.Path.join(OS.Constants.Path.profileDir, "extensions"));
  yield promiseShutdownManager();
});
