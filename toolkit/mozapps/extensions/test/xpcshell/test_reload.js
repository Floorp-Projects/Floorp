/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");
startupManager();

function* getTestAddon(fixtureName, addonID) {
  yield promiseInstallAllFiles([do_get_addon(fixtureName)]);
  restartManager();
  return promiseAddonByID(addonID);
}

function* tearDownAddon(addon) {
  addon.uninstall();
  yield promiseShutdownManager();
}

add_task(function* test_reloading_an_addon() {
  const addonId = "webextension1@tests.mozilla.org";
  const addon = yield getTestAddon("webextension_1", addonId);

  const onDisabled = promiseAddonEvent("onDisabled");
  const onEnabled = promiseAddonEvent("onEnabled");

  yield addon.reload();

  const [disabledAddon] = yield onDisabled;
  do_check_eq(disabledAddon.id, addonId);

  const [enabledAddon] = yield onEnabled;
  do_check_eq(enabledAddon.id, addonId);

  tearDownAddon(addon);
});

add_task(function* test_cannot_reload_addon_requiring_restart() {
  // This is a plain install.rdf add-on without a bootstrap script.
  const addon = yield getTestAddon("test_install1", "addon1@tests.mozilla.org");

  let caughtError = null;
  try {
    yield addon.reload();
  } catch (error) {
    caughtError = error;
  }

  do_check_eq(
    caughtError && caughtError.message,
    "cannot reload add-on because it requires a browser restart");

  tearDownAddon(addon);
});

add_task(function* test_cannot_reload_app_disabled_addon() {
  // This add-on will be app disabled immediately.
  const addon = yield getTestAddon(
    "test_bug1261522_app_disabled",
    "bug1261522-app-disabled@tests.mozilla.org");
  do_check_eq(addon.appDisabled, true);

  let caughtError = null;
  try {
    yield addon.reload();
  } catch (error) {
    caughtError = error;
  }

  do_check_eq(
    caughtError && caughtError.message,
    "cannot reload add-on because it is disabled by the application");

  tearDownAddon(addon);
});
