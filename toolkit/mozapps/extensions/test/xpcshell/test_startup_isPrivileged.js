/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ADDON_ID_PRIVILEGED = "@privileged-addon-id";
const ADDON_ID_NO_PRIV = "@addon-without-privileges";
AddonTestUtils.usePrivilegedSignatures = id => id === ADDON_ID_PRIVILEGED;

function isExtensionPrivileged(addonId) {
  const { extension } = WebExtensionPolicy.getByID(addonId);
  return extension.isPrivileged;
}

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function isPrivileged_at_install() {
  {
    let addon = await promiseInstallWebExtension({
      manifest: {
        permissions: ["mozillaAddons"],
        browser_specific_settings: { gecko: { id: ADDON_ID_PRIVILEGED } },
      },
    });
    ok(addon.isPrivileged, "Add-on is privileged");
    ok(isExtensionPrivileged(ADDON_ID_PRIVILEGED), "Extension is privileged");
  }

  {
    let addon = await promiseInstallWebExtension({
      manifest: {
        permissions: ["mozillaAddons"],
        browser_specific_settings: { gecko: { id: ADDON_ID_NO_PRIV } },
      },
    });
    ok(!addon.isPrivileged, "Add-on is not privileged");
    ok(!isExtensionPrivileged(ADDON_ID_NO_PRIV), "Extension is not privileged");
  }
});

// When the Add-on Manager is restarted, the extension is started using data
// from XPIState. This test verifies that `extension.isPrivileged` is correctly
// set in that scenario.
add_task(async function isPrivileged_at_restart() {
  await promiseRestartManager();
  {
    let addon = await AddonManager.getAddonByID(ADDON_ID_PRIVILEGED);
    ok(addon.isPrivileged, "Add-on is privileged");
    ok(isExtensionPrivileged(ADDON_ID_PRIVILEGED), "Extension is privileged");
  }
  {
    let addon = await AddonManager.getAddonByID(ADDON_ID_NO_PRIV);
    ok(!addon.isPrivileged, "Add-on is not privileged");
    ok(!isExtensionPrivileged(ADDON_ID_NO_PRIV), "Extension is not privileged");
  }
});
