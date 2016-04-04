/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "webextension1@tests.mozilla.org";

const PREF_SELECTED_LOCALE = "general.useragent.locale";

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");
startupManager();

const { GlobalManager, Management } = Components.utils.import("resource://gre/modules/Extension.jsm", {});

function promiseAddonStartup() {
  return new Promise(resolve => {
    let listener = (extension) => {
      Management.off("startup", listener);
      resolve(extension);
    };

    Management.on("startup", listener);
  });
}

function promiseInstallWebExtension(aData) {
  let addonFile = createTempWebExtensionFile(aData);

  return promiseInstallAllFiles([addonFile]).then(() => {
    Services.obs.notifyObservers(addonFile, "flush-cache-entry", null);
    return promiseAddonStartup();
  }).then(() => {
    return promiseAddonByID(aData.id);
  });
}

add_task(function*() {
  do_check_eq(GlobalManager.count, 0);
  do_check_false(GlobalManager.extensionMap.has(ID));

  yield Promise.all([
    promiseInstallAllFiles([do_get_addon("webextension_1")], true),
    promiseAddonStartup()
  ]);

  do_check_eq(GlobalManager.count, 1);
  do_check_true(GlobalManager.extensionMap.has(ID));

  let chromeReg = AM_Cc["@mozilla.org/chrome/chrome-registry;1"].
                  getService(AM_Ci.nsIChromeRegistry);
  try {
    chromeReg.convertChromeURL(NetUtil.newURI("chrome://webex/content/webex.xul"));
    do_throw("Chrome manifest should not have been registered");
  }
  catch (e) {
    // Expected the chrome url to not be registered
  }

  let addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Web Extension Name");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_false(addon.isSystem);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_SIGNED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let uri = do_get_addon_root_uri(profileDir, ID);

  do_check_eq(addon.iconURL, uri + "icon48.png");
  do_check_eq(addon.icon64URL, uri + "icon64.png");

  // Should persist through a restart
  yield promiseShutdownManager();

  do_check_eq(GlobalManager.count, 0);
  do_check_false(GlobalManager.extensionMap.has(ID));

  startupManager();
  yield promiseAddonStartup();

  do_check_eq(GlobalManager.count, 1);
  do_check_true(GlobalManager.extensionMap.has(ID));

  addon = yield promiseAddonByID(ID);
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

  do_check_eq(GlobalManager.count, 0);
  do_check_false(GlobalManager.extensionMap.has(ID));

  addon.userDisabled = false;
  yield promiseAddonStartup();

  do_check_eq(GlobalManager.count, 1);
  do_check_true(GlobalManager.extensionMap.has(ID));

  addon.uninstall();

  do_check_eq(GlobalManager.count, 0);
  do_check_false(GlobalManager.extensionMap.has(ID));

  yield promiseShutdownManager();
});

// Writing the manifest direct to the profile should work
add_task(function*() {
  writeWebManifestForExtension({
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
  yield promiseAddonStartup();

  let addon = yield promiseAddonByID(ID);
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

  yield promiseRestartManager();
});

add_task(function* test_manifest_localization() {
  const ID = "webextension3@tests.mozilla.org";

  yield promiseInstallAllFiles([do_get_addon("webextension_3")], true);
  yield promiseAddonStartup();

  let addon = yield promiseAddonByID(ID);
  addon.userDisabled = true;

  equal(addon.name, "Web Extensiøn foo ☹");
  equal(addon.description, "Descriptïon bar ☹ of add-on");

  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "fr-FR");
  yield promiseRestartManager();

  addon = yield promiseAddonByID(ID);

  equal(addon.name, "Web Extensiøn le foo ☺");
  equal(addon.description, "Descriptïon le bar ☺ of add-on");

  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "de");
  yield promiseRestartManager();

  addon = yield promiseAddonByID(ID);

  equal(addon.name, "Web Extensiøn foo ☹");
  equal(addon.description, "Descriptïon bar ☹ of add-on");

  addon.uninstall();
});

// Missing ID should cause a failure
add_task(function*() {
  writeWebManifestForExtension({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 2,
  }, profileDir, ID);

  yield promiseRestartManager();

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, null);

  let file = getFileForAddon(profileDir, ID);
  do_check_false(file.exists());

  yield promiseRestartManager();
});

// Missing version should cause a failure
add_task(function*() {
  writeWebManifestForExtension({
    name: "Web Extension Name",
    manifest_version: 2,
    applications: {
      gecko: {
        id: ID
      }
    }
  }, profileDir);

  yield promiseRestartManager();

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, null);

  let file = getFileForAddon(profileDir, ID);
  do_check_false(file.exists());

  yield promiseRestartManager();
});

// Incorrect manifest version should cause a failure
add_task(function*() {
  writeWebManifestForExtension({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 1,
    applications: {
      gecko: {
        id: ID
      }
    }
  }, profileDir);

  yield promiseRestartManager();

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, null);

  let file = getFileForAddon(profileDir, ID);
  do_check_false(file.exists());

  yield promiseRestartManager();
});

// install.rdf should be read before manifest.json
add_task(function*() {

  yield Promise.all([
    promiseInstallAllFiles([do_get_addon("webextension_2")], true)
  ]);

  yield promiseRestartManager();

  let installrdf_id = "first-webextension2@tests.mozilla.org";
  let first_addon = yield promiseAddonByID(installrdf_id);
  do_check_neq(first_addon, null);
  do_check_false(first_addon.appDisabled);
  do_check_true(first_addon.isActive);
  do_check_false(first_addon.isSystem);

  let manifestjson_id= "last-webextension2@tests.mozilla.org";
  let last_addon = yield promiseAddonByID(manifestjson_id);
  do_check_eq(last_addon, null);

  yield promiseRestartManager();
});

// Test that the "options_ui" manifest section is processed correctly.
add_task(function* test_options_ui() {
  let OPTIONS_RE = /^moz-extension:\/\/[\da-f]{8}-[\da-f]{4}-[\da-f]{4}-[\da-f]{4}-[\da-f]{12}\/options\.html$/;

  let addon = yield promiseInstallWebExtension({
    manifest: {
      "options_ui": {
        "page": "options.html",
      },
    },
  });

  equal(addon.optionsType, AddonManager.OPTIONS_TYPE_INLINE_BROWSER,
        "Addon should have an INLINE_BROWSER options type");

  ok(OPTIONS_RE.test(addon.optionsURL),
     "Addon should have a moz-extension: options URL for /options.html");

  addon.uninstall();

  addon = yield promiseInstallWebExtension({
    manifest: {
      "options_ui": {
        "page": "options.html",
        "open_in_tab": true,
      },
    },
  });

  equal(addon.optionsType, AddonManager.OPTIONS_TYPE_TAB,
        "Addon should have a TAB options type");

  ok(OPTIONS_RE.test(addon.optionsURL),
     "Addon should have a moz-extension: options URL for /options.html");

  addon.uninstall();
});
