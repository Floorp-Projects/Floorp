/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/ClientID.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

function run_test() {
  do_get_profile();
  run_next_test();
}

add_task(function* () {
  const drsPath = OS.Path.join(OS.Constants.Path.profileDir, "datareporting", "state.json");
  const fhrDir  = OS.Path.join(OS.Constants.Path.profileDir, "healthreport");
  const fhrPath = OS.Path.join(fhrDir, "state.json");
  const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
  const invalidIDs = [-1, 0.5, "INVALID-UUID", true, "", "3d1e1560-682a-4043-8cf2-aaaaaaaaaaaZ"];
  const PREF_CACHED_CLIENTID = "toolkit.telemetry.cachedClientID";

  yield OS.File.makeDir(fhrDir);

  // Check that we are importing the FHR client ID.
  let clientID = CommonUtils.generateUUID();
  yield CommonUtils.writeJSON({clientID: clientID}, fhrPath);
  Assert.equal(clientID, yield ClientID.getClientID());

  // We should persist the ID in DRS now and not pick up a differing ID from FHR.
  yield ClientID._reset();
  yield CommonUtils.writeJSON({clientID: CommonUtils.generateUUID()}, fhrPath);
  Assert.equal(clientID, yield ClientID.getClientID());

  // We should be guarded against broken FHR data.
  for (let invalidID of invalidIDs) {
    yield ClientID._reset();
    yield OS.File.remove(drsPath);
    yield CommonUtils.writeJSON({clientID: invalidID}, fhrPath);
    clientID = yield ClientID.getClientID();
    Assert.equal(typeof(clientID), 'string');
    Assert.ok(uuidRegex.test(clientID));
  }

  // We should be guarded against invalid FHR json.
  yield ClientID._reset();
  yield OS.File.remove(drsPath);
  yield OS.File.writeAtomic(fhrPath, "abcd", {encoding: "utf-8", tmpPath: fhrPath + ".tmp"});
  clientID = yield ClientID.getClientID();
  Assert.equal(typeof(clientID), 'string');
  Assert.ok(uuidRegex.test(clientID));

  // We should be guarded against broken DRS data too and fall back to loading
  // the FHR ID.
  for (let invalidID of invalidIDs) {
    yield ClientID._reset();
    clientID = CommonUtils.generateUUID();
    yield CommonUtils.writeJSON({clientID: clientID}, fhrPath);
    yield CommonUtils.writeJSON({clientID: invalidID}, drsPath);
    Assert.equal(clientID, yield ClientID.getClientID());
  }

  // We should be guarded against invalid DRS json too.
  yield ClientID._reset();
  yield OS.File.remove(fhrPath);
  yield OS.File.writeAtomic(drsPath, "abcd", {encoding: "utf-8", tmpPath: drsPath + ".tmp"});
  clientID = yield ClientID.getClientID();
  Assert.equal(typeof(clientID), 'string');
  Assert.ok(uuidRegex.test(clientID));

  // If both the FHR and DSR data are broken, we should end up with a new client ID.
  for (let invalidID of invalidIDs) {
    yield ClientID._reset();
    yield CommonUtils.writeJSON({clientID: invalidID}, fhrPath);
    yield CommonUtils.writeJSON({clientID: invalidID}, drsPath);
    clientID = yield ClientID.getClientID();
    Assert.equal(typeof(clientID), 'string');
    Assert.ok(uuidRegex.test(clientID));
  }

  // Assure that cached IDs are being checked for validity.
  for (let invalidID of invalidIDs) {
    yield ClientID._reset();
    Preferences.set(PREF_CACHED_CLIENTID, invalidID);
    let cachedID = ClientID.getCachedClientID();
    Assert.strictEqual(cachedID, null, "ClientID should ignore invalid cached IDs");
    let prefID = Preferences.get(PREF_CACHED_CLIENTID, null);
    Assert.strictEqual(prefID, null, "ClientID should reset invalid cached IDs");
  }
});
