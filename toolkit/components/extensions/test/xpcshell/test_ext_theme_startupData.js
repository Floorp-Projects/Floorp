/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

// LightweightThemeManager.fallbackThemeData setter calls are recorded here:
const gFallbackHistory = [];

// Simulates opening a browser window and retrieves the color.
async function getColorFromAppliedTheme() {
  const { LightweightThemeConsumer } = ChromeUtils.importESModule(
    "resource://gre/modules/LightweightThemeConsumer.sys.mjs"
  );

  const { ThemeVariableMap } = ChromeUtils.importESModule(
    "resource:///modules/ThemeVariableMap.sys.mjs"
  );

  // The LightweightThemeConsumer class expects a document (browser.xhtml), so
  // create a minimal one that looks like it:
  const browser = Services.appShell.createWindowlessBrowser(true);
  const document = browser.document;
  // The _setProperties function in LightweightThemeConsumer.sys.mjs expects
  // some elements to exist in order to apply the style. Make sure that these
  // exist to prevent doc.getElementById(optionalElementID) from returning null.
  for (let [, { optionalElementID }] of ThemeVariableMap) {
    if (optionalElementID) {
      let elem = document.createElement("div");
      elem.id = optionalElementID;
      document.body.append(elem);
    }
  }

  let promise = new Promise(resolve => {
    // Will be invoked by LightweightThemeConsumer
    document.defaultView.addEventListener("windowlwthemeupdate", resolve, {
      once: true,
    });
  });

  new LightweightThemeConsumer(document);

  info("Waiting for windowlwthemeupdate from LightweightThemeConsumer");
  await promise;
  info("Got windowlwthemeupdate from LightweightThemeConsumer");
  // This is the color for "colors.frame".
  let accentcolor =
    document.documentElement.style.getPropertyValue("--lwt-accent-color");
  browser.close();
  return accentcolor;
}

function loadStaticThemeExtension(manifest) {
  const extensionId = manifest.browser_specific_settings.gecko.id;
  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "permanent",
  });
  // When a theme is installed, it starts off in disabled mode, as seen in
  //  toolkit/mozapps/extensions/test/xpcshell/test_update_theme.js .
  // To make sure that ExtensionTestUtils.loadExtension resolves, enable it
  // during installation.
  extension.onInstalling = function (addon) {
    if (addon.id === extensionId) {
      equal(addon.userDisabled, true, "Static theme is disabled at install");
      // We're modifying the AddonInternal's userDisabled flag instead of
      // calling addon.enable(), because using addon.enable() during
      // onInstalling results in the Extension's startup() to be called twice,
      // which is not realistic in the real world.
      addon.__AddonInternal__.userDisabled = false;
      equal(addon.userDisabled, false, "Static theme force enabled at install");
    }
  };
  return extension;
}

add_setup(function setup_intercept_fallbackThemeData() {
  const { LightweightThemeManager } = ChromeUtils.importESModule(
    "resource://gre/modules/LightweightThemeManager.sys.mjs"
  );
  const fallbackThemeDataPropertyDescriptor = Object.getOwnPropertyDescriptor(
    LightweightThemeManager,
    "fallbackThemeData"
  );
  Object.defineProperty(LightweightThemeManager, "fallbackThemeData", {
    enumerable: true,
    configurable: true,
    set: function (value) {
      gFallbackHistory.push(structuredClone(value));
      console.trace(`Set fallbackThemeData: ${value?.theme?.id}`);
      fallbackThemeDataPropertyDescriptor.set.call(this, value);
    },
    get: fallbackThemeDataPropertyDescriptor.get,
  });
});

add_setup(async () => {
  await ExtensionTestUtils.startAddonManager();
  // In reality, loading a built-in static theme would populate the fallback.
  // But we don't have a built-in static theme in this test.
  Assert.deepEqual(gFallbackHistory, [], "Fallback history is empty");
});

async function do_test_static_theme_startupData(expectDark) {
  const EXPECTED_COLOR = expectDark ? "rgb(65, 43, 21)" : "rgb(12, 34, 56)";

  let extension = loadStaticThemeExtension({
    browser_specific_settings: { gecko: { id: "@my-theme" } },
    theme: { colors: { frame: "rgb(12, 34, 56)" } },
    dark_theme: { colors: { frame: "rgb(65, 43, 21)" } },
  });
  await extension.startup();
  let startupDataOriginal = extension.extension.startupData;
  let startupDataCopy = structuredClone(extension.extension.startupData);

  equal(
    await getColorFromAppliedTheme(),
    EXPECTED_COLOR,
    "Theme applied to simulated browser window"
  );

  equal(
    extension.extension.startupData,
    startupDataOriginal,
    "startupData should be the same value (extension not unloaded)"
  );
  Assert.deepEqual(
    extension.extension.startupData,
    startupDataCopy,
    "startupData should not have been changed when a browser window was opened"
  );

  Assert.deepEqual(
    gFallbackHistory,
    [startupDataCopy.lwtData],
    "Fallback set when theme has loaded"
  );
  gFallbackHistory.length = 0;

  let readyPromise = AddonTestUtils.promiseWebExtensionStartup("@my-theme");
  info("Simulating browser restart");
  await AddonTestUtils.promiseShutdownManager();

  Assert.deepEqual(
    gFallbackHistory,
    [],
    "Should not have bothered with resetting the fallback upon app shutdown"
  );
  gFallbackHistory.length = 0;

  await AddonTestUtils.promiseStartupManager();
  info("Waiting for theme to have started again");
  await readyPromise;

  let startupData = extension.extension.startupData;
  // It cannot be the same object because it should have been loaded from disk.
  notEqual(startupData, startupDataOriginal, "Not trivially equal");

  Assert.deepEqual(
    startupDataCopy,
    startupData,
    "startupData should be identical when the theme loads again"
  );
  ok(startupData.lwtData, "startupData.lwtData should be set");
  // Expected to be set 2x: first as soon as the static theme extension has been
  // initialized by the AddonManager, secondly when the extension internals have
  // loaded and parsed the actual theme data.
  Assert.deepEqual(
    gFallbackHistory,
    [startupData.lwtData, startupData.lwtData],
    "Should have set startupData.lwtData as fallback at startup (2x)"
  );
  gFallbackHistory.length = 0;

  equal(
    await getColorFromAppliedTheme(),
    EXPECTED_COLOR,
    "Theme applied to simulated browser window after a restart"
  );

  await extension.unload();
  Assert.deepEqual(
    gFallbackHistory,
    [null],
    "Should have cleared fallback when static theme unloaded (theme uninstall)"
  );
  gFallbackHistory.length = 0;
}

add_task(
  { pref_set: [["ui.systemUsesDarkTheme", 0]] },
  async function test_static_theme_startupData_light_theme() {
    await do_test_static_theme_startupData(/* expectDark */ false);
  }
);

add_task(
  { pref_set: [["ui.systemUsesDarkTheme", 1]] },
  async function test_static_theme_startupData_dark_theme() {
    await do_test_static_theme_startupData(/* expectDark */ true);
  }
);

// Regression test for bug 1830144.
add_task(async function test_dynamic_theme_startupData() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      browser_specific_settings: { gecko: { id: "@my-dynamic-theme" } },
      permissions: ["theme"],
    },
    useAddonManager: "permanent",
    background() {
      browser.runtime.onInstalled.addListener(async () => {
        await browser.theme.update({ colors: { frame: "rgb(7, 8, 9)" } });
        browser.test.sendMessage("onInstalled");
      });
    },
  });
  await extension.startup();
  await extension.awaitMessage("onInstalled");

  equal(
    await getColorFromAppliedTheme(),
    "rgb(7, 8, 9)",
    "Dynamic theme applies to simulated browser window"
  );

  let startupData = extension.extension.startupData;

  // Notably, startupData.lwtData (among other properties) is not set. In a
  // previous test (test_static_theme_startupData) we have confirmed that this
  // property is present in startupData of static themes.
  Assert.deepEqual(
    Object.keys(startupData),
    ["persistentListeners"],
    "startupData should not have unexpected properties from ext-theme.js"
  );

  assertPersistentListeners(extension, "runtime", "onInstalled", {
    primed: false,
    persisted: true,
  });

  await extension.unload();
});

// Due to bug 1830144, startupData of an extension may contain stale data, which
// is not automatically dropped on updates (unlike StartupCache).
// Double-check that this does not break anything.
add_task(async function test_obsolete_theme_in_extension_startupData() {
  // We're going to load a fake theme to borrow its startupData.
  let theme = loadStaticThemeExtension({
    browser_specific_settings: { gecko: { id: "@theme-ext" } },
    theme: { colors: { frame: "rgb(1, 33, 7)" } },
  });
  await theme.startup();
  let startupDataFromStaticTheme = structuredClone(theme.extension.startupData);
  await theme.unload();

  // test_static_theme_startupData already tests the expected behavior for
  // static themes, so don't bother checking again and just reset the history.
  gFallbackHistory.length = 0;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: { gecko: { id: "@theme-ext" } },
      permissions: ["theme"],
    },
    useAddonManager: "permanent",
  });
  await extension.startup();

  // The extension does not have anything of interest (such as persistent
  // listeners), so startupData should be empty.
  Assert.deepEqual(extension.extension.startupData, {}, "startupData is empty");
  extension.extension.startupData = startupDataFromStaticTheme;
  extension.extension.saveStartupData();

  let readyPromise = AddonTestUtils.promiseWebExtensionStartup("@theme-ext");
  await AddonTestUtils.promiseShutdownManager();
  await AddonTestUtils.promiseStartupManager();
  await readyPromise;
  await extension.unload();

  Assert.deepEqual(
    gFallbackHistory,
    [],
    "startupData.lwtData should be ignored for extensions that are not themes"
  );
});
