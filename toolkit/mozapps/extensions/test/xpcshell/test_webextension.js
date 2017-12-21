/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/AppConstants.jsm");

const ID = "webextension1@tests.mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");
startupManager();

const { GlobalManager } = Components.utils.import("resource://gre/modules/Extension.jsm", {});

add_task(async function() {
  equal(GlobalManager.extensionMap.size, 0);

  await Promise.all([
    promiseInstallAllFiles([do_get_addon("webextension_1")], true),
    promiseWebExtensionStartup()
  ]);

  equal(GlobalManager.extensionMap.size, 1);
  ok(GlobalManager.extensionMap.has(ID));

  let chromeReg = AM_Cc["@mozilla.org/chrome/chrome-registry;1"].
                  getService(AM_Ci.nsIChromeRegistry);
  try {
    chromeReg.convertChromeURL(NetUtil.newURI("chrome://webex/content/webex.xul"));
    do_throw("Chrome manifest should not have been registered");
  } catch (e) {
    // Expected the chrome url to not be registered
  }

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Web Extension Name");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.ok(!addon.isSystem);
  Assert.equal(addon.type, "extension");
  Assert.ok(addon.isWebExtension);
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let uri = do_get_addon_root_uri(profileDir, ID);

  Assert.equal(addon.iconURL, uri + "icon48.png");
  Assert.equal(addon.icon64URL, uri + "icon64.png");

  // Should persist through a restart
  await promiseShutdownManager();

  equal(GlobalManager.extensionMap.size, 0);

  startupManager();
  await promiseWebExtensionStartup();

  equal(GlobalManager.extensionMap.size, 1);
  ok(GlobalManager.extensionMap.has(ID));

  addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Web Extension Name");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.ok(!addon.isSystem);
  Assert.equal(addon.type, "extension");
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let file = getFileForAddon(profileDir, ID);
  Assert.ok(file.exists());

  uri = do_get_addon_root_uri(profileDir, ID);

  Assert.equal(addon.iconURL, uri + "icon48.png");
  Assert.equal(addon.icon64URL, uri + "icon64.png");

  addon.userDisabled = true;

  equal(GlobalManager.extensionMap.size, 0);

  addon.userDisabled = false;
  await promiseWebExtensionStartup();

  equal(GlobalManager.extensionMap.size, 1);
  ok(GlobalManager.extensionMap.has(ID));

  addon.uninstall();

  equal(GlobalManager.extensionMap.size, 0);
  Assert.ok(!GlobalManager.extensionMap.has(ID));

  await promiseShutdownManager();
});

// Writing the manifest direct to the profile should work
add_task(async function() {
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

  startupManager();
  await promiseWebExtensionStartup();

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Web Extension Name");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.ok(!addon.isSystem);
  Assert.equal(addon.type, "extension");
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let file = getFileForAddon(profileDir, ID);
  Assert.ok(file.exists());

  addon.uninstall();

  await promiseRestartManager();
});

add_task(async function test_manifest_localization() {
  const extensionId = "webextension3@tests.mozilla.org";

  await promiseInstallAllFiles([do_get_addon("webextension_3")], true);
  await promiseWebExtensionStartup();

  let addon = await promiseAddonByID(extensionId);
  addon.userDisabled = true;

  equal(addon.name, "Web Extensiøn foo ☹");
  equal(addon.description, "Descriptïon bar ☹ of add-on");

  Services.locale.setRequestedLocales(["fr-FR"]);
  await promiseRestartManager();

  addon = await promiseAddonByID(extensionId);

  equal(addon.name, "Web Extensiøn le foo ☺");
  equal(addon.description, "Descriptïon le bar ☺ of add-on");

  Services.locale.setRequestedLocales(["de"]);
  await promiseRestartManager();

  addon = await promiseAddonByID(extensionId);

  equal(addon.name, "Web Extensiøn foo ☹");
  equal(addon.description, "Descriptïon bar ☹ of add-on");

  addon.uninstall();
});

// Missing version should cause a failure
add_task(async function() {
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
add_task(async function() {
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

// install.rdf should be read before manifest.json
add_task(async function() {

  await Promise.all([
    promiseInstallAllFiles([do_get_addon("webextension_2")], true)
  ]);

  await promiseRestartManager();

  let installrdf_id = "first-webextension2@tests.mozilla.org";
  let first_addon = await promiseAddonByID(installrdf_id);
  Assert.notEqual(first_addon, null);
  Assert.ok(!first_addon.appDisabled);
  Assert.ok(first_addon.isActive);
  Assert.ok(!first_addon.isSystem);

  let manifestjson_id = "last-webextension2@tests.mozilla.org";
  let last_addon = await promiseAddonByID(manifestjson_id);
  Assert.equal(last_addon, null);

  await promiseRestartManager();
});

// Test that the "options_ui" manifest section is processed correctly.
add_task(async function test_options_ui() {
  let OPTIONS_RE = /^moz-extension:\/\/[\da-f]{8}-[\da-f]{4}-[\da-f]{4}-[\da-f]{4}-[\da-f]{12}\/options\.html$/;

  const extensionId = "webextension@tests.mozilla.org";
  await promiseInstallWebExtension({
    manifest: {
      applications: {gecko: {id: extensionId}},
      "options_ui": {
        "page": "options.html",
      },
    },
  });

  let addon = await promiseAddonByID(extensionId);
  equal(addon.optionsType, AddonManager.OPTIONS_TYPE_INLINE_BROWSER,
        "Addon should have an INLINE_BROWSER options type");

  ok(OPTIONS_RE.test(addon.optionsURL),
     "Addon should have a moz-extension: options URL for /options.html");

  addon.uninstall();

  const ID2 = "webextension2@tests.mozilla.org";
  await promiseInstallWebExtension({
    manifest: {
      applications: {gecko: {id: ID2}},
      "options_ui": {
        "page": "options.html",
        "open_in_tab": true,
      },
    },
  });

  addon = await promiseAddonByID(ID2);
  equal(addon.optionsType, AddonManager.OPTIONS_TYPE_TAB,
        "Addon should have a TAB options type");

  ok(OPTIONS_RE.test(addon.optionsURL),
     "Addon should have a moz-extension: options URL for /options.html");

  addon.uninstall();
});

// Test that experiments permissions add the appropriate dependencies.
add_task(async function test_experiments_dependencies() {
  let addonFile = createTempWebExtensionFile({
    manifest: {
      applications: {gecko: {id: "meh@experiment"}},
      "permissions": ["experiments.meh"],
    },
  });

  await promiseInstallAllFiles([addonFile]);

  let addon = await AddonManager.getAddonByID("meh@experiment");

  deepEqual(addon.dependencies, ["meh@experiments.addons.mozilla.org"],
            "Addon should have the expected dependencies");

  equal(addon.appDisabled, true, "Add-on should be app disabled due to missing dependencies");

  addon.uninstall();
});

// Test that experiments API extensions install correctly.
add_task(async function test_experiments_api() {
  const extensionId = "meh@experiments.addons.mozilla.org";

  let addonFile = createTempXPIFile({
    id: extensionId,
    type: 256,
    version: "0.1",
    name: "Meh API",
  });

  await promiseInstallAllFiles([addonFile]);

  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  let addon = addons.pop();
  equal(addon.id, extensionId, "Add-on should be installed as an API extension");

  addons = await AddonManager.getAddonsByTypes(["extension"]);
  equal(addons.pop().id, extensionId, "Add-on type should be aliased to extension");

  addon.uninstall();
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

  addon = await promiseAddonByID(addon.id);
  equal(addon.creator, "en name");
  equal(addon.homepageURL, "https://example.net/en");
  addon.uninstall();
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

    addon = await promiseAddonByID(addon.id);
    equal(addon.creator, "Some author");
    equal(addon.homepageURL, "https://example.net");
    addon.uninstall();
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

    addon = await promiseAddonByID(addon.id);
    equal(addon.creator, null);
    addon.uninstall();
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

  addon = await promiseAddonByID(addon.id);
  Assert.notEqual(addon, null);
  Assert.equal(addon.creator, "Some author");
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Web Extension Name");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.ok(addon.userDisabled);
  Assert.ok(!addon.isSystem);
  Assert.equal(addon.type, "theme");
  Assert.ok(addon.isWebExtension);
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  addon.uninstall();

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

  addon = await promiseAddonByID(addon.id);
  Assert.equal(addon.type, "extension");
  Assert.ok(addon.isWebExtension);

  addon.uninstall();
});

// Test that we can update from a webextension to a webextension-theme
add_task(async function test_theme_upgrade() {
  // First install a regular webextension
  let webext = createTempWebExtensionFile({
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

  await Promise.all([
    AddonManager.installTemporaryAddon(webext),
    promiseWebExtensionStartup(),
  ]);
  let addon = await promiseAddonByID(ID);

  // temporary add-on is installed and started
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test WebExtension 1 (temporary)");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  // Create a webextension theme with the same ID
  let webext2 = createTempWebExtensionFile({
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

  await Promise.all([
    AddonManager.installTemporaryAddon(webext2),
    promiseWebExtensionStartup(),
  ]);
  addon = await promiseAddonByID(ID);

  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "2.0");
  Assert.equal(addon.name, "Test WebExtension 1 (temporary)");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  // This is what we're really interested in:
  Assert.equal(addon.type, "theme");
  Assert.ok(addon.isWebExtension);

  addon.uninstall();

  addon = await promiseAddonByID(ID);
  Assert.equal(addon, null);
});
