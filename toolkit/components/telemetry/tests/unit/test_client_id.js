/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ClientID } = ChromeUtils.import("resource://gre/modules/ClientID.jsm");
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const PREF_CACHED_CLIENTID = "toolkit.telemetry.cachedClientID";

const SCALAR_DELETION_REQUEST_ECOSYSTEM_CLIENT_ID =
  "deletion.request.ecosystem_client_id";

var drsPath;

const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

function run_test() {
  do_get_profile();
  drsPath = OS.Path.join(
    OS.Constants.Path.profileDir,
    "datareporting",
    "state.json"
  );
  run_next_test();
}

add_task(async function test_ecosystemClientID() {
  await ClientID._reset();
  Assert.ok(!ClientID.getCachedEcosystemClientID());
  let ecosystemClientID = await ClientID.getEcosystemClientID();
  Assert.equal(typeof ecosystemClientID, "string");
  Assert.equal(ClientID.getCachedEcosystemClientID(), ecosystemClientID);

  let clientID = await ClientID.getClientID();
  await ClientID._reset();
  await OS.File.writeAtomic(
    drsPath,
    JSON.stringify({
      clientID,
    }),
    {
      encoding: "utf-8",
      tmpPath: drsPath + ".tmp",
    }
  );

  let newClientID = await ClientID.getClientID();
  Assert.equal(newClientID, clientID);

  let newEcosystemClientID = await ClientID.getEcosystemClientID();
  Assert.notEqual(newEcosystemClientID, ecosystemClientID);
});

add_task(async function() {
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
  Assert.equal(typeof clientID, "string");
  Assert.ok(uuidRegex.test(clientID));

  // We should be guarded against invalid DRS json.
  await ClientID._reset();
  await OS.File.writeAtomic(drsPath, "abcd", {
    encoding: "utf-8",
    tmpPath: drsPath + ".tmp",
  });
  clientID = await ClientID.getClientID();
  Assert.equal(typeof clientID, "string");
  Assert.ok(uuidRegex.test(clientID));

  // If the DRS data is broken, we should end up with a new client ID.
  for (let [invalidID] of invalidIDs) {
    await ClientID._reset();
    await CommonUtils.writeJSON({ clientID: invalidID }, drsPath);
    clientID = await ClientID.getClientID();
    Assert.equal(typeof clientID, "string");
    Assert.ok(uuidRegex.test(clientID));
  }

  // Assure that cached IDs are being checked for validity.
  for (let [invalidID, prefFunc] of invalidIDs) {
    await ClientID._reset();
    Services.prefs[prefFunc](PREF_CACHED_CLIENTID, invalidID);
    let cachedID = ClientID.getCachedClientID();
    Assert.strictEqual(
      cachedID,
      null,
      "ClientID should ignore invalid cached IDs"
    );
    Assert.ok(
      !Services.prefs.prefHasUserValue(PREF_CACHED_CLIENTID),
      "ClientID should reset invalid cached IDs"
    );
    Assert.ok(
      Services.prefs.getPrefType(PREF_CACHED_CLIENTID) ==
        Ci.nsIPrefBranch.PREF_INVALID,
      "ClientID should reset invalid cached IDs"
    );
  }
});

add_task(async function test_setCanaryClientIDs() {
  const KNOWN_UUID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0";

  await ClientID._reset();

  // We should be able to set a valid UUID
  await ClientID.setCanaryClientIDs();
  let clientID = await ClientID.getClientID();
  Assert.equal(KNOWN_UUID, clientID);
});

add_task(async function test_removeClientIDs() {
  // We should get a valid UUID after reset
  await ClientID._reset();
  let firstClientID = await ClientID.getClientID();
  let firstEcosystemClientID = await ClientID.getEcosystemClientID();
  Assert.equal(typeof firstClientID, "string");
  Assert.equal(typeof firstEcosystemClientID, "string");
  Assert.ok(uuidRegex.test(firstClientID));
  Assert.ok(uuidRegex.test(firstEcosystemClientID));

  await ClientID.removeClientIDs();

  if (
    AppConstants.platform != "android" &&
    AppConstants.MOZ_APP_NAME != "thunderbird"
  ) {
    // We don't record the old ecosystem client ID on Android or Thunderbird,
    // since the FxA and telemetry infrastructure is different there.
    let prefClientID = Services.prefs.getStringPref(PREF_CACHED_CLIENTID, null);
    let scalarsDeletionRequest = Services.telemetry.getSnapshotForScalars(
      "deletion-request"
    );
    Assert.ok(!prefClientID);
    Assert.ok(
      !scalarsDeletionRequest.parent?.[
        SCALAR_DELETION_REQUEST_ECOSYSTEM_CLIENT_ID
      ]
    );
  }

  // When resetting again we should get a new ID
  let nextClientID = await ClientID.getClientID();
  let nextEcosystemClientID = await ClientID.getEcosystemClientID();
  Assert.equal(typeof nextClientID, "string");
  Assert.equal(typeof nextEcosystemClientID, "string");
  Assert.ok(uuidRegex.test(nextClientID));
  Assert.ok(uuidRegex.test(nextEcosystemClientID));
  Assert.notEqual(
    firstClientID,
    nextClientID,
    "After reset client ID should be different."
  );
  Assert.notEqual(
    firstEcosystemClientID,
    nextEcosystemClientID,
    "After reset ecosystem client ID should be different."
  );

  let cachedID = ClientID.getCachedClientID();
  Assert.equal(nextClientID, cachedID);

  let cachedEcosystemID = ClientID.getCachedEcosystemClientID();
  Assert.equal(nextEcosystemClientID, cachedEcosystemID);

  let prefClientID = Services.prefs.getStringPref(PREF_CACHED_CLIENTID, null);
  Assert.equal(nextClientID, prefClientID);

  if (
    AppConstants.platform != "android" &&
    AppConstants.MOZ_APP_NAME != "thunderbird"
  ) {
    let scalarsDeletionRequest = Services.telemetry.getSnapshotForScalars(
      "deletion-request"
    );
    Assert.equal(
      nextEcosystemClientID,
      scalarsDeletionRequest.parent[SCALAR_DELETION_REQUEST_ECOSYSTEM_CLIENT_ID]
    );
  }
});

add_task(async function test_removeParallelGet() {
  // We should get a valid UUID after reset
  await ClientID.removeClientIDs();
  let firstClientID = await ClientID.getClientID();

  // We should get the same ID twice when requesting it in parallel to a reset.
  let promiseRemoveClientIDs = ClientID.removeClientIDs();
  let p = ClientID.getClientID();
  let newClientID = await ClientID.getClientID();
  await promiseRemoveClientIDs;
  let otherClientID = await p;

  Assert.notEqual(
    firstClientID,
    newClientID,
    "After reset client ID should be different."
  );
  Assert.equal(
    newClientID,
    otherClientID,
    "Getting the client ID in parallel to a reset should give the same id."
  );
});

add_task(
  {
    skip_if: () => AppConstants.platform != "android",
  },
  async function test_FennecCanaryDetect() {
    const KNOWN_UUID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0";

    // We should get a valid UUID after reset
    await ClientID.removeClientIDs();
    let firstClientID = await ClientID.getClientID();
    Assert.notEqual(KNOWN_UUID, firstClientID, "Client ID should be random.");

    // Set the canary client ID.
    await ClientID.setCanaryClientIDs();
    Assert.equal(
      KNOWN_UUID,
      await ClientID.getClientID(),
      "Client ID should be known canary."
    );

    await ClientID.removeClientIDs();
    let newClientID = await ClientID.getClientID();
    Assert.notEqual(
      KNOWN_UUID,
      newClientID,
      "After reset Client ID should be random."
    );
    Assert.notEqual(
      firstClientID,
      newClientID,
      "After reset Client ID should be new."
    );
    Assert.ok(
      ClientID.wasCanaryClientID(),
      "After reset we should have detected a canary client ID"
    );

    await ClientID.removeClientIDs();
    let clientID = await ClientID.getClientID();
    Assert.notEqual(
      KNOWN_UUID,
      clientID,
      "After reset Client ID should be random."
    );
    Assert.notEqual(
      newClientID,
      clientID,
      "After reset Client ID should be new."
    );
    Assert.ok(
      !ClientID.wasCanaryClientID(),
      "After reset we should not have detected a canary client ID"
    );
  }
);
