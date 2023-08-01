"use strict";

ChromeUtils.defineLazyGetter(this, "Management", () => {
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs"
  );
  return ExtensionParent.apiManager;
});

add_task(async function setup() {
  let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
  Services.prefs.setIntPref("extensions.enabledScopes", scopes);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42.0", "42.0");

  await promiseStartupManager();
});

// Verify that a theme can be updated without issues.
add_task(async function test_update_of_disabled_theme() {
  const id = "theme-only@test";
  async function installTheme(version) {
    // Upon installing a theme, it is disabled by default. Because of this,
    // ExtensionTestUtils.loadExtension cannot be used because it awaits the
    // startup of a test extension.
    let xpi = AddonTestUtils.createTempWebExtensionFile({
      manifest: {
        browser_specific_settings: { gecko: { id } },
        version,
        theme: {},
      },
    });
    let install = await AddonManager.getInstallForFile(xpi);
    let addon = await install.install();
    ok(addon.userDisabled, "Theme is expected to be disabled by default");
    equal(addon.userPermissions, null, "theme has no userPermissions");
  }

  await installTheme("1.0");

  let updatePromise = new Promise(resolve => {
    Management.on("update", function listener(name, { id: updatedId }) {
      Management.off("update", listener);
      equal(updatedId, id, "expected theme update");
      resolve();
    });
  });
  await installTheme("2.0");
  await updatePromise;
  let addon = await promiseAddonByID(id);
  equal(addon.version, "2.0", "Theme should be updated");
  ok(addon.userDisabled, "Theme is still disabled after an update");
  await addon.uninstall();
});

add_task(async function test_builtin_location_migration() {
  const ADDON_ID = "mytheme@mozilla.org";

  let themeDef = {
    manifest: {
      browser_specific_settings: { gecko: { id: ADDON_ID } },
      version: "1.0",
      theme: {},
    },
  };

  await setupBuiltinExtension(themeDef, "first-loc", false);
  await AddonManager.maybeInstallBuiltinAddon(
    ADDON_ID,
    "1.0",
    "resource://first-loc/"
  );

  let addon = await AddonManager.getAddonByID(ADDON_ID);
  await addon.enable();
  Assert.ok(!addon.userDisabled, "Add-on should be enabled.");

  Assert.equal(
    Services.prefs.getCharPref("extensions.activeThemeID", ""),
    ADDON_ID,
    "Pref should be set."
  );

  let { addons: activeThemes } = await AddonManager.getActiveAddons(["theme"]);
  Assert.equal(activeThemes.length, 1, "Should have 1 theme.");
  Assert.equal(activeThemes[0].id, ADDON_ID, "Should have enabled the theme.");

  // If we restart and update, and install a newer version of the theme,
  // it should be activated.
  await promiseShutdownManager();

  // Force schema change and restart
  Services.prefs.setIntPref("extensions.databaseSchema", 0);
  await promiseStartupManager();

  // Set up a new version of the builtin add-on.
  let newDef = { manifest: Object.assign({}, themeDef.manifest) };
  newDef.manifest.version = "1.1";
  await setupBuiltinExtension(newDef, "second-loc");
  await AddonManager.maybeInstallBuiltinAddon(
    ADDON_ID,
    "1.1",
    "resource://second-loc/"
  );

  let newAddon = await AddonManager.getAddonByID(ADDON_ID);
  Assert.ok(!newAddon.userDisabled, "Add-on should be enabled.");

  ({ addons: activeThemes } = await AddonManager.getActiveAddons(["theme"]));
  Assert.equal(activeThemes.length, 1, "Should still have 1 theme.");
  Assert.equal(
    activeThemes[0].id,
    ADDON_ID,
    "Should still have the theme enabled."
  );
  Assert.equal(
    Services.prefs.getCharPref("extensions.activeThemeID", ""),
    ADDON_ID,
    "Pref should still be set."
  );
  await promiseShutdownManager();
});
