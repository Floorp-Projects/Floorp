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

// Tests below have webextension background scripts inline.
/* globals browser */

// add-on registers upgrade listener, and ignores update.
add_task(async function() {
  // discard system addon updates
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "");

  let xpi = await getSystemAddonXPI(1, "1.0");
  xpi.copyTo(distroDir, "system1@tests.mozilla.org.xpi");

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
  let addon_postponed = await promiseAddonByID(IGNORE_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  // other addons in the set are delayed as well.
  addon_postponed = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  // restarting allows upgrades to proceed
  await Promise.all([promiseRestartManager(), extension.awaitStartup()]);

  let addon_upgraded = await promiseAddonByID(IGNORE_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  addon_upgraded = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await promiseShutdownManager();
});

// add-on registers upgrade listener, and allows update.
add_task(async function() {
  // discard system addon updates
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "");

  let xpi = await getSystemAddonXPI(1, "1.0");
  xpi.copyTo(distroDir, "system1@tests.mozilla.org.xpi");

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
  let addon_allowed = await promiseAddonByID(COMPLETE_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "1.0");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  addon_allowed = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "1.0");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // We should see that the onUpdateListener executed, then see the
  // update resume.
  await Promise.all([
    extension.awaitMessage("got-update"),
    promiseInstallResumed(COMPLETE_ID, NORMAL_ID),
    installSystemAddons(buildSystemAddonUpdates(updateList)),
  ]);

  // addon upgrade has been allowed
  addon_allowed = await promiseAddonByID(COMPLETE_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // other upgrades in the set are allowed as well
  addon_allowed = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // restarting changes nothing
  await Promise.all([promiseRestartManager(), extension.awaitStartup()]);

  let addon_upgraded = await promiseAddonByID(COMPLETE_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  addon_upgraded = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await promiseShutdownManager();
});

function delayBackground() {
  browser.test.onMessage.addListener(msg => {
    browser.runtime.reload();
    browser.test.sendMessage("reloaded");
  });
  browser.runtime.onUpdateAvailable.addListener(async function listener() {
    browser.runtime.onUpdateAvailable.removeListener(listener);
    browser.test.sendMessage("got-update");
  });
}

// Upgrade listener initially defers then proceeds after a pause.
add_task(async function() {
  // discard system addon updates
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "");

  let xpi = await getSystemAddonXPI(1, "1.0");
  xpi.copyTo(distroDir, "system1@tests.mozilla.org.xpi");

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
  let addon_postponed = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  // other addons in the set are postponed as well.
  addon_postponed = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  let deferred = promiseInstallDeferred(DEFER_ID, NORMAL_ID);

  // Tell the extension to proceed with the update.
  extension.sendMessage("go");

  await Promise.all([deferred, extension.awaitMessage("reloaded")]);

  // addon upgrade has been allowed
  let addon_allowed = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // other addons in the set are allowed as well.
  addon_allowed = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // restarting changes nothing
  await promiseRestartManager();

  let addon_upgraded = await promiseAddonByID(DEFER_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  addon_upgraded = await promiseAddonByID(NORMAL_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await promiseShutdownManager();
});

// Multiple add-ons register update listeners, initially defers then
// each unblock in turn.
add_task(async function() {
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
  let addon_postponed = await promiseAddonByID(DEFER2_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  // other addons in the set are postponed as well.
  addon_postponed = await promiseAddonByID(DEFER_ALSO_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  let deferred = promiseInstallDeferred(DEFER2_ID, DEFER_ALSO_ID);

  // Let one extension request that the update proceed.
  extension1.sendMessage("go");
  await extension1.awaitMessage("reloaded");

  // Upgrade blockers still present.
  addon_postponed = await promiseAddonByID(DEFER2_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  addon_postponed = await promiseAddonByID(DEFER_ALSO_ID);
  Assert.notEqual(addon_postponed, null);
  Assert.equal(addon_postponed.version, "1.0");
  Assert.ok(addon_postponed.isCompatible);
  Assert.ok(!addon_postponed.appDisabled);
  Assert.ok(addon_postponed.isActive);
  Assert.equal(addon_postponed.type, "extension");

  // Let the second extension allow the update to proceed.
  extension2.sendMessage("go");

  await Promise.all([
    extension2.awaitMessage("reloaded"),
    deferred,
    extension1.awaitStartup(),
    extension2.awaitStartup(),
  ]);

  // addon upgrade has been allowed
  let addon_allowed = await promiseAddonByID(DEFER2_ID);
  Assert.notEqual(addon_allowed, null);
  Assert.equal(addon_allowed.version, "2.0");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // other addons in the set are allowed as well.
  addon_allowed = await promiseAddonByID(DEFER_ALSO_ID);
  Assert.notEqual(addon_allowed, null);
  // do_check_eq(addon_allowed.version, "2.0");
  Assert.ok(addon_allowed.isCompatible);
  Assert.ok(!addon_allowed.appDisabled);
  Assert.ok(addon_allowed.isActive);
  Assert.equal(addon_allowed.type, "extension");

  // restarting changes nothing
  await Promise.all([
    promiseRestartManager(),
    extension1.awaitStartup(),
    extension2.awaitStartup(),
  ]);

  let addon_upgraded = await promiseAddonByID(DEFER2_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  addon_upgraded = await promiseAddonByID(DEFER_ALSO_ID);
  Assert.notEqual(addon_upgraded, null);
  Assert.equal(addon_upgraded.version, "2.0");
  Assert.ok(addon_upgraded.isCompatible);
  Assert.ok(!addon_upgraded.appDisabled);
  Assert.ok(addon_upgraded.isActive);
  Assert.equal(addon_upgraded.type, "extension");

  await promiseShutdownManager();
});
