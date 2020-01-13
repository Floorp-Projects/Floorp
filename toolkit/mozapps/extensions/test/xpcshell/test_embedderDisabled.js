/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ADDON_ID = "embedder-disabled@tests.mozilla.org";
const PREF_IS_EMBEDDED = "extensions.isembedded";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

// Setting PREF_DISABLE_SECURITY tells the policy engine that we are in testing
// mode and enables restarting the policy engine without restarting the browser.
registerCleanupFunction(() => {
  Services.prefs.clearUserPref(PREF_DISABLE_SECURITY);
  Services.prefs.clearUserPref(PREF_IS_EMBEDDED);
});

async function installExtension() {
  return promiseInstallWebExtension({
    manifest: {
      applications: { gecko: { id: ADDON_ID } },
    },
  });
}

add_task(async function test_setup() {
  Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, true);
  await promiseStartupManager();
});

add_task(async function embedder_disabled_while_not_embedding() {
  const addon = await installExtension();
  let exceptionThrown = false;
  try {
    await addon.setEmbedderDisabled(true);
  } catch (exception) {
    exceptionThrown = true;
  }

  equal(exceptionThrown, true);

  // Verify that the addon is not affected
  equal(addon.isActive, true);
  equal(addon.embedderDisabled, undefined);

  await addon.uninstall();
});

add_task(async function unset_embedder_disabled_while_not_embedding() {
  Services.prefs.setBoolPref(PREF_IS_EMBEDDED, true);

  const addon = await installExtension();
  await addon.setEmbedderDisabled(true);

  // Verify the addon is not active anymore
  equal(addon.isActive, false);
  equal(addon.embedderDisabled, true);

  Services.prefs.setBoolPref(PREF_IS_EMBEDDED, false);

  // Verify that embedder disabled cannot be read if not embedding
  equal(addon.embedderDisabled, undefined);

  await addon.disable();
  await addon.enable();

  // Verify that embedder disabled can be removed
  equal(addon.isActive, true);
  equal(addon.embedderDisabled, undefined);

  await addon.uninstall();
});

add_task(async function embedder_disabled_while_embedding() {
  Services.prefs.setBoolPref(PREF_IS_EMBEDDED, true);

  const addon = await installExtension();
  await addon.setEmbedderDisabled(true);

  // Verify the addon is not active anymore
  equal(addon.embedderDisabled, true);
  equal(addon.isActive, false);

  await addon.setEmbedderDisabled(false);

  // Verify that the addon is now enabled again
  equal(addon.isActive, true);
  equal(addon.embedderDisabled, false);
  await addon.uninstall();

  Services.prefs.setBoolPref(PREF_IS_EMBEDDED, false);
});

add_task(async function embedder_disabled_while_user_disabled() {
  Services.prefs.setBoolPref(PREF_IS_EMBEDDED, true);

  const addon = await installExtension();
  await addon.disable();

  // Verify that the addon is userDisabled
  equal(addon.isActive, false);
  equal(addon.userDisabled, true);
  equal(addon.embedderDisabled, false);

  await addon.setEmbedderDisabled(true);

  // Verify that the addon can be userDisabled and embedderDisabled
  equal(addon.isActive, false);
  equal(addon.userDisabled, true);
  equal(addon.embedderDisabled, true);

  await addon.setEmbedderDisabled(false);

  // Verify that unsetting embedderDisabled doesn't enable the addon
  equal(addon.isActive, false);
  equal(addon.userDisabled, true);
  equal(addon.embedderDisabled, false);

  await addon.enable();

  // Verify that the addon can be enabled after unsetting userDisabled
  equal(addon.isActive, true);
  equal(addon.userDisabled, false);
  equal(addon.embedderDisabled, false);

  await addon.uninstall();

  Services.prefs.setBoolPref(PREF_IS_EMBEDDED, false);
});
