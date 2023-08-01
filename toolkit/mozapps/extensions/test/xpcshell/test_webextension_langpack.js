/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { ExtensionUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionUtils.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "resourceProtocol", () =>
  Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler)
);

Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

const ID = "langpack-und@test.mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");

// Langpacks versions follow the following convention:
// <firefox major>.<firefox minor>.YYYYMMDD.HHmmss
// with no leading zeros allowed (as enforced per version format, see MDN doc page at https://mzl.la/3M6L15y).
//
// See https://searchfox.org/mozilla-central/rev/26790fe/python/mozbuild/mozbuild/action/langpack_manifest.py#388-398
var server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });
AddonTestUtils.registerJSON(server, "/test_update_langpack.json", {
  addons: {
    "langpack-und@test.mozilla.org": {
      updates: [
        {
          version: "58.0.20230105.121014",
          applications: {
            gecko: {
              strict_min_version: "58.0",
              strict_max_version: "58.*",
            },
          },
        },
        {
          version: "60.0.20230207.112555",
          update_link:
            "http://example.com/addons/langpack-und@test.mozilla.org.xpi",
          applications: {
            gecko: {
              strict_min_version: "60.0",
              strict_max_version: "60.*",
            },
          },
        },
        {
          version: "60.1.20230309.91233",
          update_link:
            "http://example.com/addons/dotrelease/langpack-und@test.mozilla.org.xpi",
          applications: {
            gecko: {
              strict_min_version: "60.0",
              strict_max_version: "60.*",
            },
          },
        },
      ],
    },
  },
});

// A second update url, which is included in the last of the langpack
// version from the previous one (and used to cover the staging of a
// langpack from one dotrelease to another).
AddonTestUtils.registerJSON(server, "/test_update_langpack2.json", {
  addons: {
    "langpack-und@test.mozilla.org": {
      updates: [
        {
          version: "60.2.20230319.94511",
          update_link:
            "http://example.com/addons/dotrelease2/langpack-und@test.mozilla.org.xpi",
          applications: {
            gecko: {
              strict_min_version: "60.0",
              strict_max_version: "60.*",
            },
          },
        },
      ],
    },
  },
});

function promisePostponeInstall(install) {
  return new Promise((resolve, reject) => {
    let listener = {
      onDownloadEnded: () => {
        install.postpone();
      },
      onInstallFailed: () => {
        install.removeListener(listener);
        reject(new Error("extension installation should not have failed"));
      },
      onInstallEnded: () => {
        install.removeListener(listener);
        reject(
          new Error(
            `extension installation should not have ended for ${install.addon.id}`
          )
        );
      },
      onInstallPostponed: () => {
        install.removeListener(listener);
        resolve();
      },
    };

    install.addListener(listener);
  });
}

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "58");

const ADDONS = {
  langpack_1: {
    "browser/localization/und/browser.ftl":
      "message-browser = Value from Browser\n",
    "localization/und/toolkit_test.ftl": "message-id1 = Value 1\n",
    "chrome/und/locale/und/global/test.properties":
      "message = Value from .properties\n",
    "manifest.json": {
      name: "und Language Pack",
      version: "58.0.20230105.121014",
      manifest_version: 2,
      browser_specific_settings: {
        gecko: {
          id: ID,
          strict_min_version: "58.0",
          strict_max_version: "58.*",
          update_url: "http://example.com/test_update_langpack.json",
        },
      },
      sources: {
        browser: {
          base_path: "browser/",
        },
      },
      langpack_id: "und",
      languages: {
        und: {
          chrome_resources: {
            global: "chrome/und/locale/und/global/",
          },
          version: "20171001190118",
        },
      },
      author: "Mozilla Localization Task Force",
      description: "Language pack for Testy for und",
    },
  },
};

// clone the extension so we can create an update.
const langpack_update = JSON.parse(JSON.stringify(ADDONS.langpack_1));
langpack_update["manifest.json"].version = "60.0.20230207.112555";
langpack_update["manifest.json"].browser_specific_settings.gecko = {
  id: ID,
  strict_min_version: "60.0",
  strict_max_version: "60.*",
  update_url: "http://example.com/test_update_langpack.json",
};

const langpack_update_dotrelease = JSON.parse(
  JSON.stringify(ADDONS.langpack_1)
);
langpack_update_dotrelease["manifest.json"].version = "60.1.20230309.91233";
langpack_update_dotrelease["manifest.json"].browser_specific_settings.gecko = {
  id: ID,
  strict_min_version: "60.0",
  strict_max_version: "60.*",
  update_url: "http://example.com/test_update_langpack2.json",
};

// Another langpack for another dot release part of the same major version as the previous one.
const langpack_update_dotrelease2 = JSON.parse(
  JSON.stringify(ADDONS.langpack_1)
);
langpack_update_dotrelease2["manifest.json"].version = "60.2.20230319.94511";
langpack_update_dotrelease2["manifest.json"].browser_specific_settings.gecko = {
  id: ID,
  strict_min_version: "60.0",
  strict_max_version: "60.*",
  update_url: "http://example.com/test_update_langpack2.json",
};

let xpi = AddonTestUtils.createTempXPIFile(langpack_update);
server.registerFile(`/addons/${ID}.xpi`, xpi);

let xpiDotRelease = AddonTestUtils.createTempXPIFile(
  langpack_update_dotrelease
);
server.registerFile(`/addons/dotrelease/${ID}.xpi`, xpiDotRelease);

let xpiDotRelease2 = AddonTestUtils.createTempXPIFile(
  langpack_update_dotrelease2
);
server.registerFile(`/addons/dotrelease2/${ID}.xpi`, xpiDotRelease2);

function promiseLangpackStartup() {
  return new Promise(resolve => {
    const EVENT = "webextension-langpack-startup";
    Services.obs.addObserver(function observer() {
      Services.obs.removeObserver(observer, EVENT);
      resolve();
    }, EVENT);
  });
}

add_task(async function setup() {
  Services.prefs.clearUserPref("extensions.startupScanScopes");
});

/**
 * This is a basic life-cycle test which verifies that
 * the language pack registers and unregisters correct
 * languages at various stages.
 */
add_task(async function test_basic_lifecycle() {
  await promiseStartupManager();

  // Make sure that `und` locale is not installed.
  equal(
    L10nRegistry.getInstance().getAvailableLocales().includes("und"),
    false,
    "und not installed"
  );
  equal(
    Services.locale.availableLocales.includes("und"),
    false,
    "und not available"
  );

  let [, { addon }] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);

  // Now make sure that `und` locale is available.
  equal(
    L10nRegistry.getInstance().getAvailableLocales().includes("und"),
    true,
    "und is installed"
  );
  equal(
    Services.locale.availableLocales.includes("und"),
    true,
    "und is available"
  );

  await addon.disable();

  // It is not available after the langpack has been disabled.
  equal(
    L10nRegistry.getInstance().getAvailableLocales().includes("und"),
    false,
    "und not installed"
  );
  equal(
    Services.locale.availableLocales.includes("und"),
    false,
    "und not available"
  );

  // This quirky code here allows us to handle a scenario where enabling the
  // addon is synchronous or asynchronous.
  await Promise.all([promiseLangpackStartup(), addon.enable()]);

  // After re-enabling it, the `und` locale is available again.
  equal(
    L10nRegistry.getInstance().getAvailableLocales().includes("und"),
    true,
    "und is installed"
  );
  equal(
    Services.locale.availableLocales.includes("und"),
    true,
    "und is available"
  );

  await addon.uninstall();

  // After the langpack has been uninstalled, no more `und` in locales.
  equal(
    L10nRegistry.getInstance().getAvailableLocales().includes("und"),
    false,
    "und not installed"
  );
  equal(
    Services.locale.availableLocales.includes("und"),
    false,
    "und not available"
  );
});

/**
 * This test verifies that registries are able to load and synchronously return
 * correct strings available in the language pack.
 */
add_task(async function test_locale_registries() {
  let [, { addon }] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);

  {
    // Toolkit string
    let bundles = L10nRegistry.getInstance().generateBundlesSync(
      ["und"],
      ["toolkit_test.ftl"]
    );
    let bundle0 = bundles.next().value;
    ok(bundle0);
    equal(bundle0.hasMessage("message-id1"), true);
  }

  {
    // Browser string
    let bundles = L10nRegistry.getInstance().generateBundlesSync(
      ["und"],
      ["browser.ftl"]
    );
    let bundle0 = bundles.next().value;
    ok(bundle0);
    equal(bundle0.hasMessage("message-browser"), true);
  }

  {
    // Test chrome package
    let reqLocs = Services.locale.requestedLocales;
    Services.locale.requestedLocales = ["und"];

    let bundle = Services.strings.createBundle(
      "chrome://global/locale/test.properties"
    );
    let entry = bundle.GetStringFromName("message");
    equal(entry, "Value from .properties");

    Services.locale.requestedLocales = reqLocs;
  }

  await addon.uninstall();
});

/**
 * This test verifies that registries are able to load and asynchronously return
 * correct strings available in the language pack.
 */
add_task(async function test_locale_registries_async() {
  let [, { addon }] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);

  {
    // Toolkit string
    let bundles = L10nRegistry.getInstance().generateBundles(
      ["und"],
      ["toolkit_test.ftl"]
    );
    let bundle0 = (await bundles.next()).value;
    equal(bundle0.hasMessage("message-id1"), true);
  }

  {
    // Browser string
    let bundles = L10nRegistry.getInstance().generateBundles(
      ["und"],
      ["browser.ftl"]
    );
    let bundle0 = (await bundles.next()).value;
    equal(bundle0.hasMessage("message-browser"), true);
  }

  await addon.uninstall();
  await promiseShutdownManager();
});

add_task(async function test_langpack_app_shutdown() {
  let langpackId = `langpack-und-${AppConstants.MOZ_BUILD_APP.replace(
    "/",
    "-"
  )}`;
  let check = (yes, msg) => {
    equal(resourceProtocol.hasSubstitution(langpackId), yes, msg);
  };

  await promiseStartupManager();

  check(false, "no initial resource substitution");

  await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);

  check(true, "langpack resource available after startup");

  await promiseShutdownManager();

  check(true, "langpack resource available after app shutdown");

  await promiseStartupManager();

  let addon = await AddonManager.getAddonByID(ID);
  await addon.uninstall();

  check(false, "langpack resource removed during shutdown for uninstall");

  await promiseShutdownManager();
});

add_task(async function test_amazing_disappearing_langpacks() {
  let check = yes => {
    equal(
      L10nRegistry.getInstance().getAvailableLocales().includes("und"),
      yes,
      "check L10nRegistry"
    );
    equal(
      Services.locale.availableLocales.includes("und"),
      yes,
      "check availableLocales"
    );
  };

  await promiseStartupManager();

  check(false);

  await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);

  check(true);

  await promiseShutdownManager();

  check(false);

  await AddonTestUtils.manuallyUninstall(AddonTestUtils.profileExtensions, ID);

  await promiseStartupManager();

  check(false);
});

/**
 * This test verifies that language pack will get disabled after app
 * gets upgraded.
 */
add_task(async function test_disable_after_app_update() {
  let [, { addon }] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);
  Assert.ok(addon.isActive);

  await promiseRestartManager("59");

  addon = await promiseAddonByID(ID);
  Assert.ok(!addon.isActive);
  Assert.ok(addon.appDisabled);

  await addon.uninstall();
  await promiseShutdownManager();
});

/**
 * This test verifies that a postponed language pack update will be
 * applied after a restart.
 */
add_task(async function test_after_app_update() {
  await promiseStartupManager("58");
  let [, { addon }] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);
  Assert.ok(addon.isActive);

  await promiseRestartManager("60");

  addon = await promiseAddonByID(ID);
  Assert.ok(!addon.isActive);
  Assert.ok(addon.appDisabled);
  Assert.equal(addon.version, "58.0.20230105.121014");

  let update = await promiseFindAddonUpdates(addon);
  Assert.ok(update.updateAvailable, "update is available");
  let install = update.updateAvailable;
  let postponed = promisePostponeInstall(install);
  install.install();
  await postponed;
  Assert.equal(
    install.state,
    AddonManager.STATE_POSTPONED,
    "install postponed"
  );

  await promiseRestartManager();

  addon = await promiseAddonByID(ID);
  Assert.ok(addon.isActive);
  Assert.equal(addon.version, "60.1.20230309.91233");

  await addon.uninstall();
  await promiseShutdownManager();
});

// Support setting the request locale.
function promiseLocaleChanged(requestedLocales) {
  let changed = ExtensionUtils.promiseObserved(
    "intl:requested-locales-changed"
  );
  Services.locale.requestedLocales = requestedLocales;
  return changed;
}

/**
 * This test verifies that an addon update for the next version can be
 * retrieved and staged for restart.
 */
add_task(async function test_staged_langpack_for_app_update() {
  let originalLocales = Services.locale.requestedLocales;

  await promiseStartupManager("58");
  let [, { addon }] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);
  Assert.ok(addon.isActive);
  await promiseLocaleChanged(["und"]);

  // Mimick a major release update happening while a
  // langpack from a dotrelease what already available
  // (and then assert that the dotrelease langpack
  // is the one staged and then installed on browser
  // restart)
  await AddonManager.stageLangpacksForAppUpdate("60");
  await promiseRestartManager("60");

  addon = await promiseAddonByID(ID);
  Assert.ok(addon.isActive);
  Assert.equal(addon.version, "60.1.20230309.91233");

  // Mimick a second dotrelease update, along with
  // staging of the langpacks released along with it
  // (then assert that the langpack for the second
  // dotrelease is staged and then installed on
  // browser restart).
  await promiseRestartManager("60.1");
  await AddonManager.stageLangpacksForAppUpdate("60.2");
  await promiseRestartManager("60.2");

  addon = await promiseAddonByID(ID);
  Assert.ok(addon.isActive);
  Assert.equal(addon.version, "60.2.20230319.94511");

  await addon.uninstall();
  await promiseShutdownManager();

  Services.locale.requestedLocales = originalLocales;
});

/**
 * This test verifies that an addon update for the next version can be
 * retrieved and staged for restart, but a restart failure falls back.
 */
add_task(async function test_staged_langpack_for_app_update_fail() {
  let originalLocales = Services.locale.requestedLocales;

  await promiseStartupManager("58");
  let [, { addon }] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);
  Assert.ok(addon.isActive);
  await promiseLocaleChanged(["und"]);

  await AddonManager.stageLangpacksForAppUpdate("60");
  await promiseRestartManager();

  addon = await promiseAddonByID(ID);
  Assert.ok(addon.isActive);
  Assert.equal(addon.version, "58.0.20230105.121014");

  await addon.uninstall();
  await promiseShutdownManager();
  Services.locale.requestedLocales = originalLocales;
});

/**
 * This test verifies that an update restart works when the langpack
 * cannot be updated.
 */
add_task(async function test_staged_langpack_for_app_update_not_found() {
  let originalLocales = Services.locale.requestedLocales;

  await promiseStartupManager("58");
  let [, { addon }] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);
  Assert.ok(addon.isActive);
  await promiseLocaleChanged(["und"]);

  await AddonManager.stageLangpacksForAppUpdate("59");
  await promiseRestartManager("59");

  addon = await promiseAddonByID(ID);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.version, "58.0.20230105.121014");

  await addon.uninstall();
  await promiseShutdownManager();
  Services.locale.requestedLocales = originalLocales;
});

/**
 * This test verifies that a compat update with an invalid max_version
 * will be disabled, at least allowing Firefox to startup without failures.
 */
add_task(async function test_staged_langpack_compat_startup() {
  let originalLocales = Services.locale.requestedLocales;

  await promiseStartupManager("58");
  let [, { addon }] = await Promise.all([
    promiseLangpackStartup(),
    AddonTestUtils.promiseInstallXPI(ADDONS.langpack_1),
  ]);
  Assert.ok(addon.isActive);
  await promiseLocaleChanged(["und"]);

  // Mimick a compatibility update
  let compatUpdate = {
    targetApplications: [
      {
        id: "toolkit@mozilla.org",
        minVersion: "58",
        maxVersion: "*",
      },
    ],
  };
  addon.__AddonInternal__.applyCompatibilityUpdate(compatUpdate);

  await promiseRestartManager("59");

  addon = await promiseAddonByID(ID);
  Assert.ok(!addon.isActive, "addon is not active after upgrade");
  ok(!addon.isCompatible, "compatibility update fixed");

  await promiseRestartManager("58");

  addon = await promiseAddonByID(ID);
  Assert.ok(addon.isActive, "addon is active after downgrade");
  ok(addon.isCompatible, "compatibility update fixed");

  await addon.uninstall();
  await promiseShutdownManager();
  Services.locale.requestedLocales = originalLocales;
});
