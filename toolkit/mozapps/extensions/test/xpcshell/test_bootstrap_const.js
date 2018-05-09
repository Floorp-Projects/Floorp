/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

add_task(async function() {
  await promiseStartupManager();

  let sawInstall = false;
  Services.obs.addObserver(function() {
    sawInstall = true;
  }, "addon-install");

  await promiseInstallAllFiles([do_get_addon("test_bootstrap_const")]);

  ok(sawInstall);
});
