/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "gDatareportingService",
  () => Cc["@mozilla.org/datareporting/service;1"]
          .getService(Ci.nsISupports)
          .wrappedJSObject);

function run_test() {
  do_get_profile();

  // Send the needed startup notifications to the datareporting service
  // to ensure that it has been initialized.
  gDatareportingService.observe(null, "app-startup", null);
  gDatareportingService.observe(null, "profile-after-change", null);

  run_next_test();
}

add_task(function* () {
  const drsPath = gDatareportingService._stateFilePath;
  const fhrDir  = OS.Path.join(OS.Constants.Path.profileDir, "healthreport");
  const fhrPath = OS.Path.join(fhrDir, "state.json");
  const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

  yield OS.File.makeDir(fhrDir);

  // Check that we are importing the FHR client ID.
  let clientID = CommonUtils.generateUUID();
  yield CommonUtils.writeJSON({clientID: clientID}, fhrPath);
  Assert.equal(clientID, yield gDatareportingService.getClientID());

  // We should persist the ID in DRS now and not pick up a differing ID from FHR.
  yield gDatareportingService._reset();
  yield CommonUtils.writeJSON({clientID: CommonUtils.generateUUID()}, fhrPath);
  Assert.equal(clientID, yield gDatareportingService.getClientID());

  // We should be guarded against broken FHR data.
  yield gDatareportingService._reset();
  yield OS.File.remove(drsPath);
  yield CommonUtils.writeJSON({clientID: -1}, fhrPath);
  clientID = yield gDatareportingService.getClientID();
  Assert.equal(typeof(clientID), 'string');
  Assert.ok(uuidRegex.test(clientID));

  // We should be guarded against invalid FHR json.
  yield gDatareportingService._reset();
  yield OS.File.remove(drsPath);
  yield OS.File.writeAtomic(fhrPath, "abcd", {encoding: "utf-8", tmpPath: fhrPath + ".tmp"});
  clientID = yield gDatareportingService.getClientID();
  Assert.equal(typeof(clientID), 'string');
  Assert.ok(uuidRegex.test(clientID));

  // We should be guarded against broken DRS data too and fall back to loading
  // the FHR ID.
  yield gDatareportingService._reset();
  clientID = CommonUtils.generateUUID();
  yield CommonUtils.writeJSON({clientID: clientID}, fhrPath);
  yield CommonUtils.writeJSON({clientID: -1}, drsPath);
  Assert.equal(clientID, yield gDatareportingService.getClientID());

  // We should be guarded against invalid DRS json too.
  yield gDatareportingService._reset();
  yield OS.File.remove(fhrPath);
  yield OS.File.writeAtomic(drsPath, "abcd", {encoding: "utf-8", tmpPath: drsPath + ".tmp"});
  clientID = yield gDatareportingService.getClientID();
  Assert.equal(typeof(clientID), 'string');
  Assert.ok(uuidRegex.test(clientID));

  // If both the FHR and DSR data are broken, we should end up with a new client ID.
  yield gDatareportingService._reset();
  yield CommonUtils.writeJSON({clientID: -1}, fhrPath);
  yield CommonUtils.writeJSON({clientID: -1}, drsPath);
  clientID = yield gDatareportingService.getClientID();
  Assert.equal(typeof(clientID), 'string');
  Assert.ok(uuidRegex.test(clientID));
});
