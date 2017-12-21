
const LEGACY_PREF = "extensions.legacy.enabled";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
startupManager();

add_task(async function test_disable() {
  let legacy = [
    {
      id: "bootstrap@tests.mozilla.org",
      name: "Bootstrap add-on",
      version: "1.0",
      bootstrap: true,
      multiprocessCompatible: true,
    },
    {
      id: "apiexperiment@tests.mozilla.org",
      name: "WebExtension Experiment",
      version: "1.0",
      type: 256,
    },
    {
      id: "theme@tests.mozilla.org",
      name: "Theme",
      version: "1.0",
      type: 4,
      internalName: "mytheme/1.0",
    },
  ];

  let nonLegacy = [
    {
      id: "webextension@tests.mozilla.org",
      manifest: {
        applications: {gecko: {id: "webextension@tests.mozilla.org"}},
      },
    },
    {
      id: "privileged@tests.mozilla.org",
      name: "Privileged Bootstrap add-on",
      version: "1.0",
      bootstrap: true,
      multiprocessCompatible: true,
    },
    {
      id: "dictionary@tests.mozilla.org",
      name: "Test Dictionary",
      version: "1.0",
      type: "64",
    }
  ];

  function makeXPI(info) {
    if (info.manifest) {
      return createTempWebExtensionFile(info);
    }

    return createTempXPIFile(Object.assign({}, info, {
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
    }));
  }

  AddonTestUtils.usePrivilegedSignatures = id => id.startsWith("privileged");

  // Start out with legacy extensions disabled, installing non-legacy
  // extensions should succeed.
  Services.prefs.setBoolPref(LEGACY_PREF, false);
  let installs = await Promise.all(nonLegacy.map(info => {
    let xpi = makeXPI(info);
    return AddonManager.getInstallForFile(xpi);
  }));
  await promiseCompleteAllInstalls(installs);
  for (let install of installs) {
    Assert.equal(install.state, AddonManager.STATE_INSTALLED);
    Assert.equal(install.error, 0);
  }
  let addons = await AddonManager.getAddonsByIDs(nonLegacy.map(a => a.id));
  for (let addon of addons) {
    Assert.equal(addon.appDisabled, false);
  }

  // And installing legacy extensions should fail
  let legacyXPIs = legacy.map(makeXPI);
  installs = await Promise.all(legacyXPIs.map(xpi => AddonManager.getInstallForFile(xpi)));

  // Yuck, the AddonInstall API is atrocious.  Installs of incompatible
  // extensions are detected when the install reaches the DOWNLOADED state
  // and the install is abandoned at that point.  Since this is a local file
  // install we just start out in the DONWLOADED state.
  for (let install of installs) {
    Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
    Assert.equal(install.addon.appDisabled, true);
  }

  // Now enable legacy extensions, and we should be able to install
  // the legacy extensions.
  Services.prefs.setBoolPref(LEGACY_PREF, true);
  installs = await Promise.all(legacyXPIs.map(xpi => AddonManager.getInstallForFile(xpi)));
  for (let install of installs) {
    Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
    Assert.equal(install.addon.appDisabled, false);
  }
  await promiseCompleteAllInstalls(installs);
  for (let install of installs) {
    Assert.equal(install.state, AddonManager.STATE_INSTALLED);
    Assert.equal(install.error, 0);
  }
  addons = await AddonManager.getAddonsByIDs(legacy.map(a => a.id));
  for (let addon of addons) {
    Assert.equal(addon.appDisabled, false);
  }

  // Flip the preference back, the legacy extensions should become disabled
  // but non-legacy extensions should remain enabled.
  Services.prefs.setBoolPref(LEGACY_PREF, false);
  addons = await AddonManager.getAddonsByIDs(nonLegacy.map(a => a.id));
  for (let addon of addons) {
    Assert.equal(addon.appDisabled, false);
    addon.uninstall();
  }
  addons = await AddonManager.getAddonsByIDs(legacy.map(a => a.id));
  for (let addon of addons) {
    Assert.equal(addon.appDisabled, true);
    addon.uninstall();
  }

  Services.prefs.clearUserPref(LEGACY_PREF);
});
