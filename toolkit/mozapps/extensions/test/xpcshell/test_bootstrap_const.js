/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
startupManager();

add_task(function*() {
  let sawInstall = false;
  Services.obs.addObserver(function() {
    sawInstall = true;
  }, "addon-install", false);

  yield promiseInstallAllFiles([do_get_addon("test_bootstrap_const")]);

  ok(sawInstall);
});
