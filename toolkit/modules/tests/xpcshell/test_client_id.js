/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/ClientID.jsm");
ChromeUtils.import("resource://services-common/utils.js");
ChromeUtils.import("resource://gre/modules/osfile.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const PREF_CACHED_CLIENTID = "toolkit.telemetry.cachedClientID";

function run_test() {
  do_get_profile();
  run_next_test();
}

add_task(async function() {
  const drsPath = OS.Path.join(OS.Constants.Path.profileDir, "datareporting", "state.json");
  const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
  const invalidIDs = [
    [-1, "setIntPref"],
    [0.5, "setIntPref"],
    ["INVALID-UUID", "setStringPref"],
    [true, "setBoolPref"],
    ["", "setStringPref"],
    ["3d1e1560-682a-4043-8cf2-aaaaaaaaaaaZ", "setStringPref"],
  ];

  // If there is no DRS file, we should get a new client ID.
  await ClientID._reset();
  let clientID = await ClientID.getClientID();
  Assert.equal(typeof(clientID), "string");
  Assert.ok(uuidRegex.test(clientID));

  // We should be guarded against invalid DRS json.
  await ClientID._reset();
  await OS.File.writeAtomic(drsPath, "abcd", {encoding: "utf-8", tmpPath: drsPath + ".tmp"});
  clientID = await ClientID.getClientID();
  Assert.equal(typeof(clientID), "string");
  Assert.ok(uuidRegex.test(clientID));

  // If the DRS data is broken, we should end up with a new client ID.
  for (let [invalidID, ] of invalidIDs) {
    await ClientID._reset();
    await CommonUtils.writeJSON({clientID: invalidID}, drsPath);
    clientID = await ClientID.getClientID();
    Assert.equal(typeof(clientID), "string");
    Assert.ok(uuidRegex.test(clientID));
  }

  // Assure that cached IDs are being checked for validity.
  for (let [invalidID, prefFunc] of invalidIDs) {
    await ClientID._reset();
    Services.prefs[prefFunc](PREF_CACHED_CLIENTID, invalidID);
    let cachedID = ClientID.getCachedClientID();
    Assert.strictEqual(cachedID, null, "ClientID should ignore invalid cached IDs");
    Assert.ok(!Services.prefs.prefHasUserValue(PREF_CACHED_CLIENTID),
              "ClientID should reset invalid cached IDs");
    Assert.ok(Services.prefs.getPrefType(PREF_CACHED_CLIENTID) == Ci.nsIPrefBranch.PREF_INVALID,
              "ClientID should reset invalid cached IDs");
  }
});

add_task(async function test_setClientID() {
  const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
  const invalidIDs = [
    -1,
    0.5,
    "INVALID-UUID",
    true,
    "",
    "3d1e1560-682a-4043-8cf2-aaaaaaaaaaaZ",
  ];
  const KNOWN_UUID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0";

  await ClientID._reset();

  // We should be able to set a valid UUID
  await ClientID.setClientID(KNOWN_UUID);
  let clientID = await ClientID.getClientID();
  Assert.equal(KNOWN_UUID, clientID);

  // Setting invalid UUIDs should always fail and not modify the client ID
  for (let invalidID of invalidIDs) {
    await ClientID._reset();
    let prevClientID = await ClientID.getClientID();
    await ClientID.setClientID(invalidID)
      .then(() => Assert.ok(false, `Invalid client ID '${invalidID}' should be rejected`))
      .catch(() => Assert.ok(true));

    clientID = await ClientID.getClientID();
    Assert.equal(typeof(clientID), "string");
    Assert.ok(uuidRegex.test(clientID));
    Assert.equal(prevClientID, clientID);
  }
});

add_task(async function test_resetClientID() {
  const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

  // We should get a valid UUID after reset
  await ClientID._reset();
  let firstClientID = await ClientID.getClientID();
  Assert.equal(typeof(firstClientID), "string");
  Assert.ok(uuidRegex.test(firstClientID));

  // When resetting again we should get a new ID
  let nextClientID = await ClientID.resetClientID();
  Assert.equal(typeof(nextClientID), "string");
  Assert.ok(uuidRegex.test(nextClientID));
  Assert.notEqual(firstClientID, nextClientID, "After reset client ID should be different.");

  let cachedID = ClientID.getCachedClientID();
  Assert.equal(nextClientID, cachedID);

  let prefClientID = Services.prefs.getStringPref(PREF_CACHED_CLIENTID, null);
  Assert.equal(nextClientID, prefClientID);
});

add_task(async function test_resetParallelGet() {
  // We should get a valid UUID after reset
  let firstClientID = await ClientID.resetClientID();

  // We should get the same ID twice when requesting it in parallel to a reset.
  let p = ClientID.resetClientID();
  let newClientID = await ClientID.getClientID();
  let otherClientID = await p;

  Assert.notEqual(firstClientID, newClientID, "After reset client ID should be different.");
  Assert.equal(newClientID, otherClientID, "Getting the client ID in parallel to a reset should give the same id.");
});
