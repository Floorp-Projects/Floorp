/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/AppConstants.jsm");

const ID = "webextension1@tests.mozilla.org";

const PREF_SELECTED_LOCALE = "general.useragent.locale";

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
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Web Extension Name");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_false(addon.isSystem);
  do_check_eq(addon.type, "extension");
  do_check_true(addon.isWebExtension);
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_SIGNED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let uri = do_get_addon_root_uri(profileDir, ID);

  do_check_eq(addon.iconURL, uri + "icon48.png");
  do_check_eq(addon.icon64URL, uri + "icon64.png");

  // Should persist through a restart
  await promiseShutdownManager();

  equal(GlobalManager.extensionMap.size, 0);

  startupManager();
  await promiseWebExtensionStartup();

  equal(GlobalManager.extensionMap.size, 1);
  ok(GlobalManager.extensionMap.has(ID));

  addon = await promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Web Extension Name");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_false(addon.isSystem);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_SIGNED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let file = getFileForAddon(profileDir, ID);
  do_check_true(file.exists());

  uri = do_get_addon_root_uri(profileDir, ID);

  do_check_eq(addon.iconURL, uri + "icon48.png");
  do_check_eq(addon.icon64URL, uri + "icon64.png");

  addon.userDisabled = true;

  equal(GlobalManager.extensionMap.size, 0);

  addon.userDisabled = false;
  await promiseWebExtensionStartup();

  equal(GlobalManager.extensionMap.size, 1);
  ok(GlobalManager.extensionMap.has(ID));

  addon.uninstall();

  equal(GlobalManager.extensionMap.size, 0);
  do_check_false(GlobalManager.extensionMap.has(ID));

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
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Web Extension Name");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_false(addon.isSystem);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_SIGNED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let file = getFileForAddon(profileDir, ID);
  do_check_true(file.exists());

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

  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "fr-FR");
  await promiseRestartManager();

  addon = await promiseAddonByID(extensionId);

  equal(addon.name, "Web Extensiøn le foo ☺");
  equal(addon.description, "Descriptïon le bar ☺ of add-on");

  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "de");
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
  do_check_eq(addon, null);

  let file = getFileForAddon(profileDir, ID);
  do_check_false(file.exists());

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
  do_check_eq(addon, null);

  let file = getFileForAddon(profileDir, ID);
  do_check_false(file.exists());

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
  do_check_neq(first_addon, null);
  do_check_false(first_addon.appDisabled);
  do_check_true(first_addon.isActive);
  do_check_false(first_addon.isSystem);

  let manifestjson_id = "last-webextension2@tests.mozilla.org";
  let last_addon = await promiseAddonByID(manifestjson_id);
  do_check_eq(last_addon, null);

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
  if (AppConstants.RELEASE_OR_BETA)
    // Experiments are not enabled on release builds.
    return;

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
  if (AppConstants.RELEASE_OR_BETA)
    // Experiments are not enabled on release builds.
    return;

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
  do_check_neq(addon, null);
  do_check_eq(addon.creator, "Some author");
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Web Extension Name");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_false(addon.isActive);
  do_check_true(addon.userDisabled);
  do_check_false(addon.isSystem);
  do_check_eq(addon.type, "theme");
  do_check_true(addon.isWebExtension);
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_SIGNED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

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
  do_check_eq(addon.type, "extension");
  do_check_true(addon.isWebExtension);

  addon.uninstall();
});
