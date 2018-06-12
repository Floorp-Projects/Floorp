/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const ID = "webextension1@tests.mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");

const ADDONS = {
  webextension_1: {
    "manifest.json": {
      "name": "Web Extension Name",
      "version": "1.0",
      "manifest_version": 2,
      "applications": {
        "gecko": {
          "id": "webextension1@tests.mozilla.org"
        }
      },
      "icons": {
        "48": "icon48.png",
        "64": "icon64.png"
      }
    },
    "chrome.manifest": "content webex ./\n"
  },
  webextension_3: {
    "manifest.json": {
      "name": "Web Extensiøn __MSG_name__",
      "description": "Descriptïon __MSG_desc__ of add-on",
      "version": "1.0",
      "manifest_version": 2,
      "default_locale": "en",
      "applications": {
        "gecko": {
          "id": "webextension3@tests.mozilla.org"
        }
      }
    },
    "_locales/en/messages.json": {
      "name": {
        "message": "foo ☹",
        "description": "foo"
      },
      "desc": {
        "message": "bar ☹",
        "description": "bar"
      }
    },
    "_locales/fr/messages.json": {
      "name": {
        "message": "le foo ☺",
        "description": "foo"
      },
      "desc": {
        "message": "le bar ☺",
        "description": "bar"
      }
    }
  },
};


let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

const { GlobalManager } = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});

add_task(async function test_1() {
  await promiseStartupManager();

  equal(GlobalManager.extensionMap.size, 0);

  let {addon} = await AddonTestUtils.promiseInstallXPI(ADDONS.webextension_1);

  equal(GlobalManager.extensionMap.size, 1);
  ok(GlobalManager.extensionMap.has(ID));

  Assert.throws(() => chromeReg.convertChromeURL(Services.io.newURI("chrome://webex/content/webex.xul")),
                error => error.result == Cr.NS_ERROR_FILE_NOT_FOUND,
                "Chrome manifest should not have been registered");

  let uri = do_get_addon_root_uri(profileDir, ID);

  checkAddon(ID, addon, {
    version: "1.0",
    name: "Web Extension Name",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    isSystem: false,
    type: "extension",
    isWebExtension: true,
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    iconURL: `${uri}icon48.png`,
    icon64URL: `${uri}icon64.png`,
  });

  // Should persist through a restart
  await promiseShutdownManager();

  equal(GlobalManager.extensionMap.size, 0);

  await promiseStartupManager();

  equal(GlobalManager.extensionMap.size, 1);
  ok(GlobalManager.extensionMap.has(ID));

  addon = await promiseAddonByID(ID);

  uri = do_get_addon_root_uri(profileDir, ID);

  checkAddon(ID, addon, {
    version: "1.0",
    name: "Web Extension Name",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    isSystem: false,
    type: "extension",
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    iconURL: `${uri}icon48.png`,
    icon64URL: `${uri}icon64.png`,
  });

  await addon.disable();

  equal(GlobalManager.extensionMap.size, 0);

  await addon.enable();

  equal(GlobalManager.extensionMap.size, 1);
  ok(GlobalManager.extensionMap.has(ID));

  await addon.uninstall();

  equal(GlobalManager.extensionMap.size, 0);
  Assert.ok(!GlobalManager.extensionMap.has(ID));

  await promiseShutdownManager();
});

// Writing the manifest direct to the profile should work
add_task(async function test_2() {
  await promiseWriteWebManifestForExtension({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 2,
    applications: {
      gecko: {
        id: ID
      }
    }
  }, profileDir);

  await promiseStartupManager();

  let addon = await promiseAddonByID(ID);
  checkAddon(ID, addon, {
    version: "1.0",
    name: "Web Extension Name",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    isSystem: false,
    type: "extension",
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
  });

  await addon.uninstall();

  await promiseRestartManager();
});

add_task(async function test_manifest_localization() {
  const extensionId = "webextension3@tests.mozilla.org";

  let {addon} = await AddonTestUtils.promiseInstallXPI(ADDONS.webextension_3);

  await addon.disable();

  checkAddon(ID, addon, {
    name: "Web Extensiøn foo ☹",
    description: "Descriptïon bar ☹ of add-on",
  });

  await restartWithLocales(["fr-FR"]);

  addon = await promiseAddonByID(extensionId);
  checkAddon(ID, addon, {
    name: "Web Extensiøn le foo ☺",
    description: "Descriptïon le bar ☺ of add-on",
  });

  await restartWithLocales(["de"]);

  addon = await promiseAddonByID(extensionId);
  checkAddon(ID, addon, {
    name: "Web Extensiøn foo ☹",
    description: "Descriptïon bar ☹ of add-on",
  });

  await addon.uninstall();
});

// Missing version should cause a failure
add_task(async function test_3() {
  await promiseWriteWebManifestForExtension({
    name: "Web Extension Name",
    manifest_version: 2,
    applications: {
      gecko: {
        id: ID
      }
    }
  }, profileDir);

  await promiseRestartManager();

  let addon = await promiseAddonByID(ID);
  Assert.equal(addon, null);

  let file = getFileForAddon(profileDir, ID);
  Assert.ok(!file.exists());

  await promiseRestartManager();
});

// Incorrect manifest version should cause a failure
add_task(async function test_4() {
  await promiseWriteWebManifestForExtension({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 1,
    applications: {
      gecko: {
        id: ID
      }
    }
  }, profileDir);

  await promiseRestartManager();

  let addon = await promiseAddonByID(ID);
  Assert.equal(addon, null);

  let file = getFileForAddon(profileDir, ID);
  Assert.ok(!file.exists());

  await promiseRestartManager();
});

// Test that the "options_ui" manifest section is processed correctly.
add_task(async function test_options_ui() {
  let OPTIONS_RE = /^moz-extension:\/\/[\da-f]{8}-[\da-f]{4}-[\da-f]{4}-[\da-f]{4}-[\da-f]{12}\/options\.html$/;

  const extensionId = "webextension@tests.mozilla.org";
  let addon = await promiseInstallWebExtension({
    manifest: {
      applications: {gecko: {id: extensionId}},
      "options_ui": {
        "page": "options.html",
      },
    },
  });

  checkAddon(extensionId, addon, {
    optionsType: AddonManager.OPTIONS_TYPE_INLINE_BROWSER,
  });

  ok(OPTIONS_RE.test(addon.optionsURL),
     "Addon should have a moz-extension: options URL for /options.html");

  await addon.uninstall();

  const ID2 = "webextension2@tests.mozilla.org";
  addon = await promiseInstallWebExtension({
    manifest: {
      applications: {gecko: {id: ID2}},
      "options_ui": {
        "page": "options.html",
        "open_in_tab": true,
      },
    },
  });

  checkAddon(ID2, addon, {
    optionsType: AddonManager.OPTIONS_TYPE_TAB,
  });

  ok(OPTIONS_RE.test(addon.optionsURL),
     "Addon should have a moz-extension: options URL for /options.html");

  await addon.uninstall();
});

// Test that experiments permissions add the appropriate dependencies.
add_task(async function test_experiments_dependencies() {
  let addon = await promiseInstallWebExtension({
    manifest: {
      applications: {gecko: {id: "meh@experiment"}},
      "permissions": ["experiments.meh"],
    },
  });

  checkAddon(addon.id, addon, {
    dependencies: ["meh@experiments.addons.mozilla.org"],
    // Add-on should be app disabled due to missing dependencies
    appDisabled: true,
  });

  await addon.uninstall();
});

add_task(async function developerShouldOverride() {
  let addon = await promiseInstallWebExtension({
    manifest: {
      default_locale: "en",
      developer: {
        name: "__MSG_name__",
        url: "__MSG_url__"
      },
      author: "Will be overridden by developer",
      homepage_url: "https://will.be.overridden",
    },
    files: {
      "_locales/en/messages.json": `{
        "name": {
          "message": "en name"
        },
        "url": {
          "message": "https://example.net/en"
        }
      }`
    }
  });

  checkAddon(ID, addon, {
    creator: "en name",
    homepageURL: "https://example.net/en",
  });

  await addon.uninstall();
});

add_task(async function developerEmpty() {
  for (let developer of [{}, null, {name: null, url: null}]) {
    let addon = await promiseInstallWebExtension({
      manifest: {
        author: "Some author",
        developer,
        homepage_url: "https://example.net",
        manifest_version: 2,
        name: "Web Extension Name",
        version: "1.0",
      }
    });

    checkAddon(ID, addon, {
      creator: "Some author",
      homepageURL: "https://example.net",
    });

    await addon.uninstall();
  }
});

add_task(async function authorNotString() {
  for (let author of [{}, [], 42]) {
    let addon = await promiseInstallWebExtension({
      manifest: {
        author,
        manifest_version: 2,
        name: "Web Extension Name",
        version: "1.0",
      }
    });

    checkAddon(ID, addon, {
      creator: null,
    });

    await addon.uninstall();
  }
});

add_task(async function testThemeExtension() {
  let addon = await promiseInstallWebExtension({
    manifest: {
      "author": "Some author",
      manifest_version: 2,
      name: "Web Extension Name",
      version: "1.0",
      theme: { images: { headerURL: "example.png" } },
    }
  });

  checkAddon(ID, addon, {
    creator: "Some author",
    version: "1.0",
    name: "Web Extension Name",
    isCompatible: true,
    appDisabled: false,
    isActive: false,
    userDisabled: true,
    isSystem: false,
    type: "theme",
    isWebExtension: true,
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
  });

  await addon.uninstall();

  // Also test one without a proper 'theme' section.
  addon = await promiseInstallWebExtension({
    manifest: {
      "author": "Some author",
      manifest_version: 2,
      name: "Web Extension Name",
      version: "1.0",
      theme: null,
    }
  });

  checkAddon(ID, addon, {
    type: "extension",
    isWebExtension: true,
  });

  await addon.uninstall();
});

// Test that we can update from a webextension to a webextension-theme
add_task(async function test_theme_upgrade() {
  // First install a regular webextension
  let addon = await promiseInstallWebExtension({
    manifest: {
      version: "1.0",
      name: "Test WebExtension 1 (temporary)",
      applications: {
        gecko: {
          id: ID
        }
      }
    }
  });

  checkAddon(ID, addon, {
    version: "1.0",
    name: "Test WebExtension 1 (temporary)",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    type: "extension",
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
  });

  // Create a webextension theme with the same ID
  addon = await promiseInstallWebExtension({
    manifest: {
      version: "2.0",
      name: "Test WebExtension 1 (temporary)",
      applications: {
        gecko: {
          id: ID
        }
      },
      theme: { images: { headerURL: "example.png" } }
    }
  });

  checkAddon(ID, addon, {
    version: "2.0",
    name: "Test WebExtension 1 (temporary)",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    // This is what we're really interested in:
    type: "theme",
    isWebExtension: true,
  });

  await addon.uninstall();

  addon = await promiseAddonByID(ID);
  Assert.equal(addon, null);
});
