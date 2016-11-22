/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

BootstrapMonitor.init();

const PREF_DB_SCHEMA = "extensions.databaseSchema";

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");
startupManager();

/**
 *  Schema change with no application update reloads metadata.
 */
add_task(function* schema_change() {
  const ID = "schema-change@tests.mozilla.org";

  let xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on",
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1.9.2"
    }]
  });

  yield promiseInstallFile(xpiFile);

  let addon = yield promiseAddonByID(ID);

  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "1.0", "Got the expected version");

  yield promiseShutdownManager();

  xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on 2",
    version: "2.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1.9.2"
    }]
  });

  Services.prefs.setIntPref(PREF_DB_SCHEMA, 0);

  let file = profileDir.clone();
  file.append(`${ID}.xpi`);

  // Make sure the timestamp is unchanged, so it is not re-scanned for that reason.
  let timestamp = file.lastModifiedTime;
  xpiFile.moveTo(profileDir, `${ID}.xpi`);

  file.lastModifiedTime = timestamp;

  yield promiseStartupManager();

  addon = yield promiseAddonByID(ID);
  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "2.0", "Got the expected version");

  let waitUninstall = promiseAddonEvent("onUninstalled");
  addon.uninstall();
  yield waitUninstall;
});

/**
 *  Application update with no schema change does not reload metadata.
 */
add_task(function* schema_change() {
  const ID = "schema-change@tests.mozilla.org";

  let xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on",
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  });

  yield promiseInstallFile(xpiFile);

  let addon = yield promiseAddonByID(ID);

  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "1.0", "Got the expected version");

  yield promiseShutdownManager();

  xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on 2",
    version: "2.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  });

  gAppInfo.version = "2";
  let file = profileDir.clone();
  file.append(`${ID}.xpi`);

  // Make sure the timestamp is unchanged, so it is not re-scanned for that reason.
  let timestamp = file.lastModifiedTime;
  xpiFile.moveTo(profileDir, `${ID}.xpi`);

  file.lastModifiedTime = timestamp;

  yield promiseStartupManager();

  addon = yield promiseAddonByID(ID);
  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "1.0", "Got the expected version");

  let waitUninstall = promiseAddonEvent("onUninstalled");
  addon.uninstall();
  yield waitUninstall;
});

/**
 *  App update and a schema change causes a reload of the manifest.
 */
add_task(function* schema_change_app_update() {
  const ID = "schema-change@tests.mozilla.org";

  let xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on",
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"
    }]
  });

  yield promiseInstallFile(xpiFile);

  let addon = yield promiseAddonByID(ID);

  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "1.0", "Got the expected version");

  yield promiseShutdownManager();

  xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on 2",
    version: "2.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"
    }]
  });

  gAppInfo.version = "3";
  Services.prefs.setIntPref(PREF_DB_SCHEMA, 0);

  let file = profileDir.clone();
  file.append(`${ID}.xpi`);

  // Make sure the timestamp is unchanged, so it is not re-scanned for that reason.
  let timestamp = file.lastModifiedTime;
  xpiFile.moveTo(profileDir, `${ID}.xpi`);

  file.lastModifiedTime = timestamp;

  yield promiseStartupManager();

  addon = yield promiseAddonByID(ID);
  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.appDisabled, false);
  equal(addon.version, "2.0", "Got the expected version");

  let waitUninstall = promiseAddonEvent("onUninstalled");
  addon.uninstall();
  yield waitUninstall;
});

/**
 *  No schema change, no manifest reload.
 */
add_task(function* schema_change() {
  const ID = "schema-change@tests.mozilla.org";

  let xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on",
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1.9.2"
    }]
  });

  yield promiseInstallFile(xpiFile);

  let addon = yield promiseAddonByID(ID);

  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "1.0", "Got the expected version");

  yield promiseShutdownManager();

  xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on 2",
    version: "2.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1.9.2"
    }]
  });

  let file = profileDir.clone();
  file.append(`${ID}.xpi`);

  // Make sure the timestamp is unchanged, so it is not re-scanned for that reason.
  let timestamp = file.lastModifiedTime;
  xpiFile.moveTo(profileDir, `${ID}.xpi`);

  file.lastModifiedTime = timestamp;

  yield promiseStartupManager();

  addon = yield promiseAddonByID(ID);
  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "1.0", "Got the expected version");

  let waitUninstall = promiseAddonEvent("onUninstalled");
  addon.uninstall();
  yield waitUninstall;
});

/**
 *  Modified timestamp on the XPI causes a reload of the manifest.
 */
add_task(function* schema_change() {
  const ID = "schema-change@tests.mozilla.org";

  let xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on",
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1.9.2"
    }]
  });

  yield promiseInstallFile(xpiFile);

  let addon = yield promiseAddonByID(ID);

  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "1.0", "Got the expected version");

  yield promiseShutdownManager();

  xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on 2",
    version: "2.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1.9.2"
    }]
  });

  xpiFile.moveTo(profileDir, `${ID}.xpi`);

  let file = profileDir.clone();
  file.append(`${ID}.xpi`);

  // Set timestamp in the future so manifest is re-scanned.
  let timestamp = new Date(Date.now() + 60000);
  xpiFile.moveTo(profileDir, `${ID}.xpi`);

  file.lastModifiedTime = timestamp;

  yield promiseStartupManager();

  addon = yield promiseAddonByID(ID);
  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "2.0", "Got the expected version");

  let waitUninstall = promiseAddonEvent("onUninstalled");
  addon.uninstall();
  yield waitUninstall;
});
