/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

Services.prefs.setBoolPref(
  "extensions.webextensions.background-delayed-startup",
  false
);

const ADDON_ID = "test-startup-cache@xpcshell.mozilla.org";

function makeExtension(opts) {
  return {
    useAddonManager: "permanent",

    manifest: {
      version: opts.version,
      applications: { gecko: { id: ADDON_ID } },

      name: "__MSG_name__",

      default_locale: "en_US",
    },

    files: {
      "_locales/en_US/messages.json": {
        name: {
          message: `en-US ${opts.version}`,
          description: "Name.",
        },
      },
      "_locales/fr/messages.json": {
        name: {
          message: `fr ${opts.version}`,
          description: "Name.",
        },
      },
    },

    background() {
      browser.test.onMessage.addListener(msg => {
        if (msg === "get-manifest") {
          browser.test.sendMessage("manifest", browser.runtime.getManifest());
        }
      });
    },
  };
}

add_task(async function() {
  Preferences.set("extensions.logging.enabled", false);
  await AddonTestUtils.promiseStartupManager();

  // Install langpacks to get proper locale startup.
  let langpack = {
    "manifest.json": {
      name: "test Language Pack",
      version: "1.0",
      manifest_version: 2,
      applications: {
        gecko: {
          id: "@test-langpack",
          strict_min_version: "42.0",
          strict_max_version: "42.0",
        },
      },
      langpack_id: "fr",
      languages: {
        fr: {
          chrome_resources: {
            global: "chrome/fr/locale/fr/global/",
          },
          version: "20171001190118",
        },
      },
      sources: {
        browser: {
          base_path: "browser/",
        },
      },
    },
  };

  let [, { addon }] = await Promise.all([
    TestUtils.topicObserved("webextension-langpack-startup"),
    AddonTestUtils.promiseInstallXPI(langpack),
  ]);

  let extension = ExtensionTestUtils.loadExtension(
    makeExtension({ version: "1.0" })
  );

  function getManifest() {
    extension.sendMessage("get-manifest");
    return extension.awaitMessage("manifest");
  }

  // At the moment extension language negotiation is tied to Firefox language
  // negotiation result. That means that to test an extension in `fr`, we need
  // to mock `fr` being available in Firefox and then request it.
  //
  // In the future, we should provide some way for tests to decouple their
  // language selection from that of Firefox.
  ok(Services.locale.availableLocales.includes("fr"), "fr locale is avialable");

  await extension.startup();

  equal(extension.version, "1.0", "Expected extension version");
  let manifest = await getManifest();
  equal(manifest.name, "en-US 1.0", "Got expected manifest name");

  info("Restart and re-check");
  await AddonTestUtils.promiseRestartManager();
  await extension.awaitStartup();

  equal(extension.version, "1.0", "Expected extension version");
  manifest = await getManifest();
  equal(manifest.name, "en-US 1.0", "Got expected manifest name");

  info("Change locale to 'fr' and restart");
  Services.locale.requestedLocales = ["fr"];
  await AddonTestUtils.promiseRestartManager();
  await extension.awaitStartup();

  equal(extension.version, "1.0", "Expected extension version");
  manifest = await getManifest();
  equal(manifest.name, "fr 1.0", "Got expected manifest name");

  info("Update to version 1.1");
  await extension.upgrade(makeExtension({ version: "1.1" }));

  equal(extension.version, "1.1", "Expected extension version");
  manifest = await getManifest();
  equal(manifest.name, "fr 1.1", "Got expected manifest name");

  info("Change locale to 'en-US' and restart");
  Services.locale.requestedLocales = ["en-US"];
  await AddonTestUtils.promiseRestartManager();
  await extension.awaitStartup();

  equal(extension.version, "1.1", "Expected extension version");
  manifest = await getManifest();
  equal(manifest.name, "en-US 1.1", "Got expected manifest name");

  info("uninstall locale 'fr'");
  addon = await AddonManager.getAddonByID("@test-langpack");
  await addon.uninstall();
  ok(!Services.locale.availableLocales.includes("fr"), "fr locale is removed");

  await extension.unload();
});
