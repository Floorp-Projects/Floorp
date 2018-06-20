/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});
const { L10nRegistry } = ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm", {});

const ID = "langpack-und@test.mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "58");

const ADDONS = {
  langpack_1: {
    "browser/localization/und/browser.ftl": "message-browser = Value from Browser\n",
    "localization/und/toolkit_test.ftl": "message-id1 = Value 1\n",
    "chrome/und/locale/und/global/test.properties": "message = Value from .properties\n",
    "manifest.json": {
      "name": "und Language Pack",
      "version": "1",
      "manifest_version": 2,
      "applications": {
        "gecko": {
          "id": "langpack-und@test.mozilla.org",
          "strict_min_version": "58.0",
          "strict_max_version": "58.*"
        }
      },
      "sources": {
        "browser": {
          "base_path": "browser/"
        }
      },
      "langpack_id": "und",
      "languages": {
        "und": {
          "chrome_resources": {
            "global": "chrome/und/locale/und/global/"
          },
          "version": "20171001190118"
        }
      },
      "author": "Mozilla Localization Task Force",
      "description": "Language pack for Testy for und"
    }
  },
};

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
  await promiseStartupManager();

  // Make sure that `und` locale is not installed.
  equal(L10nRegistry.getAvailableLocales().includes("und"), false);
  equal(Services.locale.getAvailableLocales().includes("und"), false);

  let [, {addon}] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);

  // Now make sure that `und` locale is available.
  equal(L10nRegistry.getAvailableLocales().includes("und"), true);
  equal(Services.locale.getAvailableLocales().includes("und"), true);

  await addon.disable();

  // It is not available after the langpack has been disabled.
  equal(L10nRegistry.getAvailableLocales().includes("und"), false);
  equal(Services.locale.getAvailableLocales().includes("und"), false);

  // This quirky code here allows us to handle a scenario where enabling the
  // addon is synchronous or asynchronous.
  await Promise.all([
    promiseLangpackStartup(),
    addon.enable(),
  ]);

  // After re-enabling it, the `und` locale is available again.
  equal(L10nRegistry.getAvailableLocales().includes("und"), true);
  equal(Services.locale.getAvailableLocales().includes("und"), true);

  await addon.uninstall();

  // After the langpack has been uninstalled, no more `und` in locales.
  equal(L10nRegistry.getAvailableLocales().includes("und"), false);
  equal(Services.locale.getAvailableLocales().includes("und"), false);
});

/**
 * This test verifies that registries are able to load and return
 * correct strings available in the language pack.
 */
add_task(async function() {
  let [, {addon}] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
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

  await addon.uninstall();
});

/**
 * This test verifies that language pack will get disabled after app
 * gets upgraded.
 */
add_task(async function() {
  let [, {addon}] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);
  Assert.ok(addon.isActive);

  await promiseShutdownManager();

  gAppInfo.version = "59";
  gAppInfo.platformVersion = "59";

  await promiseStartupManager();

  addon = await promiseAddonByID(ID);
  Assert.ok(!addon.isActive);
  Assert.ok(addon.appDisabled);

  await addon.uninstall();
});
