/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/ClientID.jsm");
ChromeUtils.import("resource://services-common/utils.js");
ChromeUtils.import("resource://gre/modules/osfile.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

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
  const PREF_CACHED_CLIENTID = "toolkit.telemetry.cachedClientID";

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
