/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/AppConstants.jsm");

const { Services } = Components.utils.import("resource://gre/modules/Services.jsm", {});
const { L10nRegistry } = Components.utils.import("resource://gre/modules/L10nRegistry.jsm", {});

const ID = "langpack-und@test.mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "58");
startupManager();

function promiseLangpackStartup() {
  return new Promise(resolve => {
    const EVENT = "webextension-langpack-startup";
    Services.obs.addObserver(function observer() {
      Services.obs.removeObserver(observer, EVENT);
      resolve();
    }, EVENT);
  });
}

/**
 * This is a basic life-cycle test which verifies that
 * the language pack registers and unregisters correct
 * languages at various stages.
 */
add_task(async function() {
  // Make sure that `und` locale is not installed.
  equal(L10nRegistry.getAvailableLocales().includes("und"), false);
  equal(Services.locale.getAvailableLocales().includes("und"), false);

  let [{addon}, ] = await Promise.all([
    promiseInstallFile(do_get_addon("langpack_1"), true),
    promiseLangpackStartup()
  ]);

  // Now make sure that `und` locale is available.
  equal(L10nRegistry.getAvailableLocales().includes("und"), true);
  equal(Services.locale.getAvailableLocales().includes("und"), true);

  addon.userDisabled = true;

  // It is not available after the langpack has been disabled.
  equal(L10nRegistry.getAvailableLocales().includes("und"), false);
  equal(Services.locale.getAvailableLocales().includes("und"), false);

  addon.userDisabled = false;
  await promiseLangpackStartup();

  // After re-enabling it, the `und` locale is available again.
  equal(L10nRegistry.getAvailableLocales().includes("und"), true);
  equal(Services.locale.getAvailableLocales().includes("und"), true);

  addon.uninstall();

  // After the langpack has been uninstalled, no more `und` in locales.
  equal(L10nRegistry.getAvailableLocales().includes("und"), false);
  equal(Services.locale.getAvailableLocales().includes("und"), false);
});

/**
 * This test verifies that registries are able to load and return
 * correct strings available in the language pack.
 */
add_task(async function() {
  let [{addon}, ] = await Promise.all([
    promiseInstallFile(do_get_addon("langpack_1"), true),
    promiseLangpackStartup()
  ]);

  {
    // Toolkit string
    let ctxs = L10nRegistry.generateContexts(["und"], ["toolkit_test.ftl"]);
    let ctx0 = (await ctxs.next()).value;
    equal(ctx0.hasMessage("message-id1"), true);
  }

  {
    // Browser string
    let ctxs = L10nRegistry.generateContexts(["und"], ["browser.ftl"]);
    let ctx0 = (await ctxs.next()).value;
    equal(ctx0.hasMessage("message-browser"), true);
  }

  {
    // Test chrome package
    let reqLocs = Services.locale.getRequestedLocales();
    Services.locale.setRequestedLocales(["und"]);

    let bundle = Services.strings.createBundle(
      "chrome://global/locale/test.properties"
    );
    let entry = bundle.GetStringFromName("message");
    equal(entry, "Value from .properties");

    Services.locale.setRequestedLocales(reqLocs);
  }

  addon.uninstall();
});

/**
 * This test verifies that language pack will get disabled after app
 * gets upgraded.
 */
add_task(async function() {
  let [{addon}, ] = await Promise.all([
    promiseInstallFile(do_get_addon("langpack_1"), true),
    promiseLangpackStartup()
  ]);
  Assert.ok(addon.isActive);

  await promiseShutdownManager();

  gAppInfo.version = "59";
  gAppInfo.platformVersion = "59";

  await promiseStartupManager(true);

  addon = await promiseAddonByID(ID);
  Assert.ok(!addon.isActive);
  Assert.ok(addon.appDisabled);

  addon.uninstall();
});
