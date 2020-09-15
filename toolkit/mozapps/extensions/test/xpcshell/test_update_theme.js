"use strict";

XPCOMUtils.defineLazyGetter(this, "Management", () => {
  const { ExtensionParent } = ChromeUtils.import(
    "resource://gre/modules/ExtensionParent.jsm"
  );
  return ExtensionParent.apiManager;
});

add_task(async function setup() {
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
        applications: { gecko: { id } },
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
