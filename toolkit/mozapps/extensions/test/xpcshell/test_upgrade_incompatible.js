// turn on Cu.isInAutomation
Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, true);

// Tests that when an extension manifest that was previously valid becomes
// unparseable after an application update, the extension becomes
// disabled.  (See bug 1439600 for a concrete example of a situation where
// this happened).
add_task(async function test_upgrade_incompatible() {
  const ID = "incompatible-upgrade@tests.mozilla.org";

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  await promiseStartupManager();

  let file = createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id: ID } },
    },
  });

  let { addon } = await promiseInstallFile(file);

  notEqual(addon, null);
  equal(addon.appDisabled, false);

  await promiseShutdownManager();

  // Create a new, incompatible extension
  let newfile = createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id: ID } },
      manifest_version: 1,
    },
  });

  // swap the incompatible extension in for the original
  let path = OS.Path.join(gProfD.path, "extensions", `${ID}.xpi`);
  let sb = await OS.File.stat(path);
  let timestamp = sb.lastModificationDate.valueOf();

  await OS.File.move(newfile.path, path);
  await promiseSetExtensionModifiedTime(path, timestamp);
  Services.obs.notifyObservers(new FileUtils.File(path), "flush-cache-entry");

  // Restart.  With the change to the DB schema we recompute compatibility.
  // With an unparseable manifest the addon should become disabled.
  Services.prefs.setIntPref("extensions.databaseSchema", 0);
  await promiseStartupManager();

  addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.appDisabled, true);

  await promiseShutdownManager();

  file = createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id: ID } },
    },
  });

  // swap the old extension back in and check that we don't persist the disabled state forever.
  await OS.File.move(file.path, path);
  await promiseSetExtensionModifiedTime(path, timestamp);
  Services.obs.notifyObservers(new FileUtils.File(path), "flush-cache-entry");

  // Restart.  With the change to the DB schema we recompute compatibility.
  Services.prefs.setIntPref("extensions.databaseSchema", 0);
  await promiseStartupManager();

  addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.appDisabled, false);

  await promiseShutdownManager();
});
