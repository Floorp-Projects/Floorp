/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const PREF_DB_SCHEMA = "extensions.databaseSchema";
const PREF_IS_EMBEDDED = "extensions.isembedded";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(PREF_DISABLE_SECURITY);
  Services.prefs.clearUserPref(PREF_IS_EMBEDDED);
});

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

add_task(async function test_setup() {
  Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, true);
  await promiseStartupManager();
});

add_task(async function run_tests() {
  // Fake installTelemetryInfo used in the addon installation,
  // to verify that they are preserved after the DB is updated
  // from the addon manifests.
  const fakeInstallTelemetryInfo = { source: "amo", method: "amWebAPI" };

  const ID = "schema-change@tests.mozilla.org";

  const xpi1 = createTempWebExtensionFile({
    manifest: {
      name: "Test Add-on",
      version: "1.0",
      applications: { gecko: { id: ID } },
    },
  });

  const xpi2 = createTempWebExtensionFile({
    manifest: {
      name: "Test Add-on 2",
      version: "2.0",
      applications: { gecko: { id: ID } },
    },
  });

  let xpiPath = OS.Path.join(profileDir.path, `${ID}.xpi`);

  const TESTS = [
    {
      what: "Schema change with no application update reloads metadata.",
      expectedVersion: "2.0",
      action() {
        Services.prefs.setIntPref(PREF_DB_SCHEMA, 0);
      },
    },
    {
      what:
        "Application update with no schema change does not reload metadata.",
      expectedVersion: "1.0",
      action() {
        gAppInfo.version = "2";
      },
    },
    {
      what: "App update and a schema change causes a reload of the manifest.",
      expectedVersion: "2.0",
      action() {
        gAppInfo.version = "3";
        Services.prefs.setIntPref(PREF_DB_SCHEMA, 0);
      },
    },
    {
      what: "No schema change, no manifest reload.",
      expectedVersion: "1.0",
      action() {},
    },
    {
      what: "Modified timestamp on the XPI causes a reload of the manifest.",
      expectedVersion: "2.0",
      async action() {
        let stat = await IOUtils.stat(xpiPath);
        let newLastModTime = stat.lastModified + 60 * 1000;
        await IOUtils.touch(xpiPath, newLastModTime);
      },
    },
  ];

  for (let test of TESTS) {
    info(test.what);
    await promiseInstallFile(xpi1, false, fakeInstallTelemetryInfo);

    let addon = await promiseAddonByID(ID);
    notEqual(addon, null, "Got an addon object as expected");
    equal(addon.version, "1.0", "Got the expected version");
    Assert.deepEqual(
      addon.installTelemetryInfo,
      fakeInstallTelemetryInfo,
      "Got the expected installTelemetryInfo after installing the addon"
    );

    await promiseShutdownManager();

    let fileInfo = await IOUtils.stat(xpiPath);

    xpi2.copyTo(profileDir, `${ID}.xpi`);

    // Make sure the timestamp of the extension is unchanged, so it is not
    // re-scanned for that reason.
    await IOUtils.touch(xpiPath, fileInfo.lastModified);

    await test.action();

    await promiseStartupManager();

    addon = await promiseAddonByID(ID);
    notEqual(addon, null, "Got an addon object as expected");
    equal(addon.version, test.expectedVersion, "Got the expected version");
    Assert.deepEqual(
      addon.installTelemetryInfo,
      fakeInstallTelemetryInfo,
      "Got the expected installTelemetryInfo after rebuilding the DB"
    );

    await addon.uninstall();
  }
});

add_task(async function embedder_disabled_stays_disabled() {
  Services.prefs.setBoolPref(PREF_IS_EMBEDDED, true);

  const ID = "embedder-disabled@tests.mozilla.org";

  await promiseInstallWebExtension({
    manifest: {
      name: "Test Add-on",
      version: "1.0",
      applications: { gecko: { id: ID } },
    },
  });

  let addon = await promiseAddonByID(ID);

  equal(addon.embedderDisabled, false);

  await addon.setEmbedderDisabled(true);
  equal(addon.embedderDisabled, true);

  await promiseShutdownManager();

  // Change db schema to force reload
  Services.prefs.setIntPref(PREF_DB_SCHEMA, 0);

  await promiseStartupManager();

  addon = await promiseAddonByID(ID);
  equal(addon.embedderDisabled, true);

  await addon.uninstall();
});
