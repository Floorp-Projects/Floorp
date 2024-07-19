/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

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

add_setup(async () => {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_static_theme_startupData() {
  const DUMMY_COLOR = "rgb(12, 34, 56)";

  let extension = loadStaticThemeExtension({
    browser_specific_settings: { gecko: { id: "@my-theme" } },
    theme: {
      colors: { frame: DUMMY_COLOR },
    },
  });
  await extension.startup();
  let startupDataOriginal = extension.extension.startupData;
  let startupDataCopy = structuredClone(extension.extension.startupData);

  equal(
    await getColorFromAppliedTheme(),
    DUMMY_COLOR,
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

  let readyPromise = AddonTestUtils.promiseWebExtensionStartup("@my-theme");
  info("Simulating browser restart");
  await AddonTestUtils.promiseRestartManager();
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

  await extension.unload();
});
