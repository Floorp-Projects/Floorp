/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if logins-backup.json is used correctly in the event that logins.json is missing or corrupt.
 */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "LoginStore",
  "resource://gre/modules/LoginStore.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const rawLogin1 = {
  id: 1,
  hostname: "http://www.example.com",
  httpRealm: null,
  formSubmitURL: "http://www.example.com",
  usernameField: "field_" + String.fromCharCode(533, 537, 7570, 345),
  passwordField: "field_" + String.fromCharCode(421, 259, 349, 537),
  encryptedUsername: "(test)",
  encryptedPassword: "(test)",
  guid: "(test)",
  encType: Ci.nsILoginManagerCrypto.ENCTYPE_SDR,
  timeCreated: Date.now(),
  timeLastUsed: Date.now(),
  timePasswordChanged: Date.now(),
  timesUsed: 1,
};

const rawLogin2 = {
  id: 2,
  hostname: "http://www.example2.com",
  httpRealm: null,
  formSubmitURL: "http://www.example2.com",
  usernameField: "field_2" + String.fromCharCode(533, 537, 7570, 345),
  passwordField: "field_2" + String.fromCharCode(421, 259, 349, 537),
  encryptedUsername: "(test2)",
  encryptedPassword: "(test2)",
  guid: "(test2)",
  encType: Ci.nsILoginManagerCrypto.ENCTYPE_SDR,
  timeCreated: Date.now(),
  timeLastUsed: Date.now(),
  timePasswordChanged: Date.now(),
  timesUsed: 1,
};

// Enable the collection (during test) for all products so even products
// that don't collect the data will be able to run the test without failure.
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

/**
 * Tests that logins-backup.json can be used by JSONFile.load() when logins.json is missing or cannot be read.
 */
add_task(async function test_logins_store_missing_or_corrupt_with_backup() {
  let loginsStorePath = OS.Path.join(
    OS.Constants.Path.profileDir,
    "logins.json"
  );
  let loginsStoreBackup = OS.Path.join(
    OS.Constants.Path.profileDir,
    "logins-backup.json"
  );

  // Get store.data ready.
  let store = new LoginStore(loginsStorePath, loginsStoreBackup);
  await store.load();

  // Files should not exist at start up.
  Assert.equal(false, await OS.File.exists(store.path));
  Assert.equal(false, await OS.File.exists(store._options.backupTo));

  // Add logins to create logins.json and logins-backup.json.
  store.data.logins.push(rawLogin1);
  await store._save();
  Assert.equal(true, await OS.File.exists(store.path));

  store.data.logins.push(rawLogin2);
  await store._save();
  Assert.equal(true, await OS.File.exists(store._options.backupTo));

  // Remove logins.json and see if logins-backup.json will be used.
  await OS.File.remove(store.path);
  store.data.logins = [];
  store.dataReady = false;
  Assert.equal(false, await OS.File.exists(store.path));
  Assert.equal(true, await OS.File.exists(store._options.backupTo));

  // Clear any telemetry events recorded in the jsonfile category previously.
  Services.telemetry.clearEvents();

  await store.load();

  // Important to check here if logins.json is restored as expected
  // after it went missing.
  await OS.File.exists(store.path);

  Assert.equal(true, await OS.File.exists(store._options.backupTo));
  Assert.equal(
    store.data.logins.length,
    1,
    "Logins backup was used successfully when logins.json was missing"
  );

  TelemetryTestUtils.assertEvents(
    [
      ["jsonfile", "load", "logins"],
      ["jsonfile", "load", "logins", "used_backup"],
    ],
    {},
    { clear: true }
  );
  info(
    "Telemetry was recorded accurately when logins-backup.json is used when logins.json was missing"
  );

  // Corrupt the logins.json file.
  let string = '{"logins":[{"hostname":"http://www.example.com","id":1,';
  await OS.File.writeAtomic(store.path, new TextEncoder().encode(string), {
    tmpPath: store.path + ".tmp",
  });

  // Clear events recorded in the jsonfile category previously.
  Services.telemetry.clearEvents();

  // Try to load the corrupt file.
  store.data.logins = [];
  store.dataReady = false;
  await store.load();

  // logins.json.corrupt should be created.
  await OS.File.exists(store.path + ".corrupt");

  // Important to check here if logins.json is restored as expected
  // after it was corrupted.
  await OS.File.exists(store.path);

  // Data should be loaded from logins-backup.json.
  Assert.equal(true, await OS.File.exists(store._options.backupTo));
  Assert.equal(
    store.data.logins.length,
    1,
    "Logins backup was used successfully when logins.json was corrupt"
  );

  TelemetryTestUtils.assertEvents(
    [
      ["jsonfile", "load", "logins", ""],
      ["jsonfile", "load", "logins", "invalid_json"],
      ["jsonfile", "load", "logins", "used_backup"],
    ],
    {},
    { clear: true }
  );
  info(
    "Telemetry was recorded accurately when logins-backup.json is used when logins.json was corrupt"
  );

  // Clean up before we start the second part of the test.
  await OS.File.remove(store.path + ".corrupt");

  // Test that the backup file can be used by JSONFile.ensureDataReady() correctly when logins.json is missing.
  // Remove logins.json
  await OS.File.remove(store.path);
  store.data.logins = [];
  store.dataReady = false;
  Assert.equal(false, await OS.File.exists(store.path));
  Assert.equal(true, await OS.File.exists(store._options.backupTo));

  store.ensureDataReady();

  // Important to check here if logins.json is restored as expected
  // after it went missing.
  await OS.File.exists(store.path);

  Assert.equal(true, await OS.File.exists(store._options.backupTo));
  await TestUtils.waitForCondition(() => {
    return store.data.logins.length == 1;
  });

  // Test that the backup file is used by JSONFile.ensureDataReady() when logins.json is corrupt.
  // Corrupt the logins.json file.
  await OS.File.writeAtomic(store.path, new TextEncoder().encode(string), {
    tmpPath: store.path + ".tmp",
  });

  // Try to load the corrupt file.
  store.data.logins = [];
  store.dataReady = false;
  store.ensureDataReady();

  // logins.json.corrupt should be created.
  Assert.equal(true, await OS.File.exists(store.path + ".corrupt"));

  // Important to check here if logins.json is restored as expected
  // after it was corrupted.
  await OS.File.exists(store.path);

  // Data should be loaded from logins-backup.json.
  Assert.equal(true, await OS.File.exists(store._options.backupTo));
  await TestUtils.waitForCondition(() => {
    return store.data.logins.length == 1;
  });
});
