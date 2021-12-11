/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that delaying a system add-on update works.

PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Message manager disconnected/
);

const profileDir = gProfD.clone();
profileDir.append("extensions");

const IGNORE_ID = "system_delay_ignore@tests.mozilla.org";
const COMPLETE_ID = "system_delay_complete@tests.mozilla.org";
const DEFER_ID = "system_delay_defer@tests.mozilla.org";
const DEFER2_ID = "system_delay_defer2@tests.mozilla.org";
const DEFER_ALSO_ID = "system_delay_defer_also@tests.mozilla.org";
const NORMAL_ID = "system1@tests.mozilla.org";

const distroDir = FileUtils.getDir("ProfD", ["sysfeatures"], true);
registerDirectory("XREAppFeat", distroDir);

registerCleanupFunction(() => {
  distroDir.remove(true);
});

AddonTestUtils.usePrivilegedSignatures = id => "system";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

function promiseInstallPostponed(addonID1, addonID2) {
  return new Promise((resolve, reject) => {
    let seen = [];
    let listener = {
      onInstallFailed: () => {
        AddonManager.removeInstallListener(listener);
        reject("extension installation should not have failed");
      },
      onInstallEnded: install => {
        AddonManager.removeInstallListener(listener);
        reject(
          `extension installation should not have ended for ${install.addon.id}`
        );
      },
      onInstallPostponed: install => {
        seen.push(install.addon.id);
        if (seen.includes(addonID1) && seen.includes(addonID2)) {
          AddonManager.removeInstallListener(listener);
          resolve();
        }
      },
    };

    AddonManager.addInstallListener(listener);
  });
}

function promiseInstallResumed(addonID1, addonID2) {
  return new Promise((resolve, reject) => {
    let seenPostponed = [];
    let seenEnded = [];
    let listener = {
      onInstallFailed: () => {
        AddonManager.removeInstallListener(listener);
        reject("extension installation should not have failed");
      },
      onInstallEnded: install => {
        seenEnded.push(install.addon.id);
        if (
          seenEnded.includes(addonID1) &&
          seenEnded.includes(addonID2) &&
          seenPostponed.includes(addonID1) &&
          seenPostponed.includes(addonID2)
        ) {
          AddonManager.removeInstallListener(listener);
          resolve();
        }
      },
      onInstallPostponed: install => {
        seenPostponed.push(install.addon.id);
      },
    };

    AddonManager.addInstallListener(listener);
  });
}

function promiseInstallDeferred(addonID1, addonID2) {
  return new Promise((resolve, reject) => {
    let seenEnded = [];
    let listener = {
      onInstallFailed: () => {
        AddonManager.removeInstallListener(listener);
        reject("extension installation should not have failed");
      },
      onInstallEnded: install => {
        seenEnded.push(install.addon.id);
        if (seenEnded.includes(addonID1) && seenEnded.includes(addonID2)) {
          AddonManager.removeInstallListener(listener);
          resolve();
        }
      },
    };

    AddonManager.addInstallListener(listener);
  });
}

async function checkAddon(addonID, { version }) {
  let addon = await promiseAddonByID(addonID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, version);
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
}

// Tests below have webextension background scripts inline.
/* globals browser */

// add-on registers upgrade listener, and ignores update.
add_task(async function test_addon_upgrade_on_restart() {
  // discard system addon updates
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "");

  let xpi = await getSystemAddonXPI(1, "1.0");
  xpi.copyTo(distroDir, `${NORMAL_ID}.xpi`);

  // Version 1.0 of an extension that ignores updates.
  function background() {
    browser.runtime.onUpdateAvailable.addListener(() => {
      browser.test.sendMessage("got-update");
    });
  }

  xpi = await createTempWebExtensionFile({
    background,

    manifest: {
      version: "1.0",
      applications: { gecko: { id: IGNORE_ID } },
    },
  });
  xpi.copyTo(distroDir, `${IGNORE_ID}.xpi`);

  // Version 2.0 of the same extension.
  let xpi2 = await createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: { gecko: { id: IGNORE_ID } },
    },
  });

  await overrideBuiltIns({ system: [IGNORE_ID, NORMAL_ID] });

  let extension = ExtensionTestUtils.expectExtension(IGNORE_ID);

  await Promise.all([promiseStartupManager(), extension.awaitStartup()]);

  let updateList = [
    {
      id: IGNORE_ID,
      version: "2.0",
      path: "system_delay_ignore_2.xpi",
      xpi: xpi2,
    },
    {
      id: NORMAL_ID,
      version: "2.0",
      path: "system1_2.xpi",
      xpi: await getSystemAddonXPI(1, "2.0"),
    },
  ];

  await Promise.all([
    promiseInstallPostponed(IGNORE_ID, NORMAL_ID),
    installSystemAddons(buildSystemAddonUpdates(updateList)),
    extension.awaitMessage("got-update"),
  ]);

  // addon upgrade has been delayed.
  await checkAddon(IGNORE_ID, { version: "1.0" });
  // other addons in the set are delayed as well.
  await checkAddon(NORMAL_ID, { version: "1.0" });

  // restarting allows upgrades to proceed
  await Promise.all([promiseRestartManager(), extension.awaitStartup()]);

  await checkAddon(IGNORE_ID, { version: "2.0" });
  await checkAddon(NORMAL_ID, { version: "2.0" });

  await promiseShutdownManager();
});

// add-on registers upgrade listener, and allows update.
add_task(async function test_addon_upgrade_on_reload() {
  // discard system addon updates
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "");

  let xpi = await getSystemAddonXPI(1, "1.0");
  xpi.copyTo(distroDir, `${NORMAL_ID}.xpi`);

  // Version 1.0 of an extension that listens for and immediately
  // applies updates.
  function background() {
    browser.runtime.onUpdateAvailable.addListener(function listener() {
      browser.runtime.onUpdateAvailable.removeListener(listener);
      browser.test.sendMessage("got-update");
      browser.runtime.reload();
    });
  }

  xpi = await createTempWebExtensionFile({
    background,

    manifest: {
      version: "1.0",
      applications: { gecko: { id: COMPLETE_ID } },
    },
  });
  xpi.copyTo(distroDir, `${COMPLETE_ID}.xpi`);

  // Version 2.0 of the same extension.
  let xpi2 = await createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: { gecko: { id: COMPLETE_ID } },
    },
  });

  await overrideBuiltIns({ system: [COMPLETE_ID, NORMAL_ID] });

  let extension = ExtensionTestUtils.expectExtension(COMPLETE_ID);

  await Promise.all([promiseStartupManager(), extension.awaitStartup()]);

  let updateList = [
    {
      id: COMPLETE_ID,
      version: "2.0",
      path: "system_delay_complete_2.xpi",
      xpi: xpi2,
    },
    {
      id: NORMAL_ID,
      version: "2.0",
      path: "system1_2.xpi",
      xpi: await getSystemAddonXPI(1, "2.0"),
    },
  ];

  // initial state
  await checkAddon(COMPLETE_ID, { version: "1.0" });
  await checkAddon(NORMAL_ID, { version: "1.0" });

  // We should see that the onUpdateListener executed, then see the
  // update resume.
  await Promise.all([
    extension.awaitMessage("got-update"),
    promiseInstallResumed(COMPLETE_ID, NORMAL_ID),
    installSystemAddons(buildSystemAddonUpdates(updateList)),
  ]);
  await extension.awaitStartup();

  // addon upgrade has been allowed
  await checkAddon(COMPLETE_ID, { version: "2.0" });
  // other upgrades in the set are allowed as well
  await checkAddon(NORMAL_ID, { version: "2.0" });

  // restarting changes nothing
  await Promise.all([promiseRestartManager(), extension.awaitStartup()]);

  await checkAddon(COMPLETE_ID, { version: "2.0" });
  await checkAddon(NORMAL_ID, { version: "2.0" });

  await promiseShutdownManager();
});

function delayBackground() {
  browser.test.onMessage.addListener(msg => {
    if (msg !== "reload") {
      browser.test.fail(`Got unexpected test message: ${msg}`);
    }
    browser.runtime.reload();
  });
  browser.runtime.onUpdateAvailable.addListener(async function listener() {
    browser.runtime.onUpdateAvailable.removeListener(listener);
    browser.test.sendMessage("got-update");
  });
}

// Upgrade listener initially defers then proceeds after a pause.
add_task(async function test_addon_upgrade_after_pause() {
  // discard system addon updates
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "");

  let xpi = await getSystemAddonXPI(1, "1.0");
  xpi.copyTo(distroDir, `${NORMAL_ID}.xpi`);

  // Version 1.0 of an extension that delays upgrades.
  xpi = await createTempWebExtensionFile({
    background: delayBackground,
    manifest: {
      version: "1.0",
      applications: { gecko: { id: DEFER_ID } },
    },
  });
  xpi.copyTo(distroDir, `${DEFER_ID}.xpi`);

  // Version 2.0 of the same xtension.
  let xpi2 = await createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: { gecko: { id: DEFER_ID } },
    },
  });

  await overrideBuiltIns({ system: [DEFER_ID, NORMAL_ID] });

  let extension = ExtensionTestUtils.expectExtension(DEFER_ID);

  await Promise.all([promiseStartupManager(), extension.awaitStartup()]);

  let updateList = [
    {
      id: DEFER_ID,
      version: "2.0",
      path: "system_delay_defer_2.xpi",
      xpi: xpi2,
    },
    {
      id: NORMAL_ID,
      version: "2.0",
      path: "system1_2.xpi",
      xpi: await getSystemAddonXPI(1, "2.0"),
    },
  ];

  await Promise.all([
    promiseInstallPostponed(DEFER_ID, NORMAL_ID),
    installSystemAddons(buildSystemAddonUpdates(updateList)),
    extension.awaitMessage("got-update"),
  ]);

  // upgrade is initially postponed
  await checkAddon(DEFER_ID, { version: "1.0" });
  // other addons in the set are postponed as well.
  await checkAddon(NORMAL_ID, { version: "1.0" });

  let deferred = promiseInstallDeferred(DEFER_ID, NORMAL_ID);

  // Tell the extension to proceed with the update.
  extension.setRestarting();
  extension.sendMessage("reload");

  await Promise.all([deferred, extension.awaitStartup()]);

  // addon upgrade has been allowed
  await checkAddon(DEFER_ID, { version: "2.0" });
  // other addons in the set are allowed as well.
  await checkAddon(NORMAL_ID, { version: "2.0" });

  // restarting changes nothing
  await promiseRestartManager();
  await extension.awaitStartup();

  await checkAddon(DEFER_ID, { version: "2.0" });
  await checkAddon(NORMAL_ID, { version: "2.0" });

  await promiseShutdownManager();
});

// Multiple add-ons register update listeners, initially defers then
// each unblock in turn.
add_task(async function test_multiple_addon_upgrade_postpone() {
  // discard system addon updates.
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "");

  let updateList = [];

  let xpi = await createTempWebExtensionFile({
    background: delayBackground,
    manifest: {
      version: "1.0",
      applications: { gecko: { id: DEFER2_ID } },
    },
  });
  xpi.copyTo(distroDir, `${DEFER2_ID}.xpi`);

  xpi = await createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: { gecko: { id: DEFER2_ID } },
    },
  });
  updateList.push({
    id: DEFER2_ID,
    version: "2.0",
    path: "system_delay_defer_2.xpi",
    xpi,
  });

  xpi = await createTempWebExtensionFile({
    background: delayBackground,
    manifest: {
      version: "1.0",
      applications: { gecko: { id: DEFER_ALSO_ID } },
    },
  });
  xpi.copyTo(distroDir, `${DEFER_ALSO_ID}.xpi`);

  xpi = await createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: { gecko: { id: DEFER_ALSO_ID } },
    },
  });
  updateList.push({
    id: DEFER_ALSO_ID,
    version: "2.0",
    path: "system_delay_defer_also_2.xpi",
    xpi,
  });

  await overrideBuiltIns({ system: [DEFER2_ID, DEFER_ALSO_ID] });

  let extension1 = ExtensionTestUtils.expectExtension(DEFER2_ID);
  let extension2 = ExtensionTestUtils.expectExtension(DEFER_ALSO_ID);

  await Promise.all([
    promiseStartupManager(),
    extension1.awaitStartup(),
    extension2.awaitStartup(),
  ]);

  await Promise.all([
    promiseInstallPostponed(DEFER2_ID, DEFER_ALSO_ID),
    installSystemAddons(buildSystemAddonUpdates(updateList)),
    extension1.awaitMessage("got-update"),
    extension2.awaitMessage("got-update"),
  ]);

  // upgrade is initially postponed
  await checkAddon(DEFER2_ID, { version: "1.0" });
  // other addons in the set are postponed as well.
  await checkAddon(DEFER_ALSO_ID, { version: "1.0" });

  let deferred = promiseInstallDeferred(DEFER2_ID, DEFER_ALSO_ID);

  // Let one extension request that the update proceed.
  extension1.setRestarting();
  extension1.sendMessage("reload");

  // Upgrade blockers still present.
  await checkAddon(DEFER2_ID, { version: "1.0" });
  await checkAddon(DEFER_ALSO_ID, { version: "1.0" });

  // Let the second extension allow the update to proceed.
  extension2.setRestarting();
  extension2.sendMessage("reload");

  await Promise.all([
    deferred,
    extension1.awaitStartup(),
    extension2.awaitStartup(),
  ]);

  // addon upgrade has been allowed
  await checkAddon(DEFER2_ID, { version: "2.0" });
  await checkAddon(DEFER_ALSO_ID, { version: "2.0" });

  // restarting changes nothing
  await Promise.all([
    promiseRestartManager(),
    extension1.awaitStartup(),
    extension2.awaitStartup(),
  ]);

  await checkAddon(DEFER2_ID, { version: "2.0" });
  await checkAddon(DEFER_ALSO_ID, { version: "2.0" });

  await promiseShutdownManager();
});
