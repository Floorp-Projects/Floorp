/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

"use strict";

ChromeUtils.import("resource://gre/modules/osfile.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryStorage.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://testing-common/AppData.jsm", this);

// The name of the pending pings directory outside of the user profile,
// in the user app data directory.
const PENDING_PING_DIR_NAME = "Pending Pings";

async function createFakeAppDir() {
  // Create a directory inside the profile and register it as UAppData, so
  // we can stick fake crash pings inside there. We put it inside the profile
  // just because we know that will get cleaned up after the test runs.
  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

  // Create "<profile>/UAppData/Pending Pings".
  const pendingPingsPath = OS.Path.join(
    profileDir.path,
    "UAppData",
    PENDING_PING_DIR_NAME
  );
  await OS.File.makeDir(pendingPingsPath, {
    ignoreExisting: true,
    from: OS.Constants.Path.profileDir,
  });

  await makeFakeAppDir();
}

add_task(async function setup() {
  // Init the profile.
  do_get_profile();
  await createFakeAppDir();
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();
});

add_task(async function test_migrateUnsentPings() {
  const PINGS = [
    {
      type: "crash",
      id: TelemetryUtils.generateUUID(),
      payload: { foo: "bar" },
      dateCreated: new Date(2010, 1, 1, 10, 0, 0),
    },
    {
      type: "other",
      id: TelemetryUtils.generateUUID(),
      payload: { moo: "meh" },
      dateCreated: new Date(2010, 2, 1, 10, 2, 0),
    },
  ];
  const APP_DATA_DIR = Services.dirsvc.get("UAppData", Ci.nsIFile).path;
  const APPDATA_PINGS_DIR = OS.Path.join(APP_DATA_DIR, PENDING_PING_DIR_NAME);

  // Create some pending pings outside of the user profile.
  for (let ping of PINGS) {
    const pingPath = OS.Path.join(APPDATA_PINGS_DIR, ping.id + ".json");
    await TelemetryStorage.savePingToFile(ping, pingPath, true);
  }

  // Make sure the pending ping list is empty.
  await TelemetryStorage.testClearPendingPings();

  // Start the migration from TelemetryStorage.
  let pendingPings = await TelemetryStorage.loadPendingPingList();
  Assert.equal(
    pendingPings.length,
    2,
    "TelemetryStorage must have migrated 2 pings."
  );

  for (let ping of PINGS) {
    // Verify that the pings were migrated and are among the pending pings.
    Assert.ok(
      pendingPings.find(p => p.id == ping.id),
      "The ping must have been migrated."
    );

    // Try to load the migrated ping from the user profile.
    let migratedPing = await TelemetryStorage.loadPendingPing(ping.id);
    Assert.equal(
      ping.id,
      migratedPing.id,
      "Should have loaded the correct ping id."
    );
    Assert.equal(
      ping.type,
      migratedPing.type,
      "Should have loaded the correct ping type."
    );
    Assert.deepEqual(
      ping.payload,
      migratedPing.payload,
      "Should have loaded the correct payload."
    );

    // Verify that the pings are no longer outside of the user profile.
    const pingPath = OS.Path.join(APPDATA_PINGS_DIR, ping.id + ".json");
    Assert.ok(
      !(await OS.File.exists(pingPath)),
      "The ping should not be in the Pending Pings directory anymore."
    );
  }
});

add_task(async function test_migrateIncompatiblePing() {
  const APP_DATA_DIR = Services.dirsvc.get("UAppData", Ci.nsIFile).path;
  const APPDATA_PINGS_DIR = OS.Path.join(APP_DATA_DIR, PENDING_PING_DIR_NAME);

  // Create a ping incompatible with migration outside of the user profile.
  const pingPath = OS.Path.join(APPDATA_PINGS_DIR, "incompatible.json");
  await TelemetryStorage.savePingToFile({ incom: "patible" }, pingPath, true);

  // Ensure the pending ping list is empty.
  await TelemetryStorage.testClearPendingPings();
  TelemetryStorage.reset();

  // Start the migration from TelemetryStorage.
  let pendingPings = await TelemetryStorage.loadPendingPingList();
  Assert.equal(
    pendingPings.length,
    0,
    "TelemetryStorage must have migrated no pings." +
      JSON.stringify(pendingPings)
  );

  Assert.ok(
    !(await OS.File.exists(pingPath)),
    "The incompatible ping must have been deleted by the migration"
  );
});

add_task(async function teardown() {
  // Delete the UAppData directory and make sure nothing breaks.
  const APP_DATA_DIR = Services.dirsvc.get("UAppData", Ci.nsIFile).path;
  await OS.File.removeDir(APP_DATA_DIR, { ignorePermissions: true });
  Assert.ok(
    !(await OS.File.exists(APP_DATA_DIR)),
    "The UAppData directory must not exist anymore."
  );
  TelemetryStorage.reset();
  await TelemetryStorage.loadPendingPingList();
});
