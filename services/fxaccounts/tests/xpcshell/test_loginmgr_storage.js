/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for FxAccounts, storage and the master password.

// Stop us hitting the real auth server.
Services.prefs.setCharPref("identity.fxaccounts.auth.uri", "http://localhost");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/FxAccountsCommon.js");

initTestLogging("Trace");
// See verbose logging from FxAccounts.jsm
Services.prefs.setCharPref("identity.fxaccounts.loglevel", "DEBUG");

function run_test() {
  run_next_test();
}

function getLoginMgrData() {
  let logins = Services.logins.findLogins({}, FXA_PWDMGR_HOST, null, FXA_PWDMGR_REALM);
  if (logins.length == 0) {
    return null;
  }
  Assert.equal(logins.length, 1, "only 1 login available");
  return logins[0];
}

add_task(function test_simple() {
  let fxa = new FxAccounts({});

  let creds = {
    email: "test@example.com",
    sessionToken: "sessionToken",
    kA: "the kA value",
    kB: "the kB value",
    verified: true
  };
  yield fxa.setSignedInUser(creds);

  // This should have stored stuff in both the .json file in the profile
  // dir, and the login dir.
  let path = OS.Path.join(OS.Constants.Path.profileDir, "signedInUser.json");
  let data = yield CommonUtils.readJSON(path);

  Assert.strictEqual(data.accountData.email, creds.email, "correct email in the clear text");
  Assert.strictEqual(data.accountData.sessionToken, creds.sessionToken, "correct sessionToken in the clear text");
  Assert.strictEqual(data.accountData.verified, creds.verified, "correct verified flag");

  Assert.ok(!("kA" in data.accountData), "kA not stored in clear text");
  Assert.ok(!("kB" in data.accountData), "kB not stored in clear text");

  let login = getLoginMgrData();
  Assert.strictEqual(login.username, creds.email, "email matches");
  let loginData = JSON.parse(login.password);
  Assert.strictEqual(loginData.version, data.version, "same version flag in both places");
  Assert.strictEqual(loginData.accountData.kA, creds.kA, "correct kA in the login mgr");
  Assert.strictEqual(loginData.accountData.kB, creds.kB, "correct kB in the login mgr");

  Assert.ok(!("email" in loginData), "email not stored in the login mgr json");
  Assert.ok(!("sessionToken" in loginData), "sessionToken not stored in the login mgr json");
  Assert.ok(!("verified" in loginData), "verified not stored in the login mgr json");

  yield fxa.signOut(/* localOnly = */ true);
  Assert.strictEqual(getLoginMgrData(), null, "login mgr data deleted on logout");
});

add_task(function test_MPLocked() {
  let fxa = new FxAccounts({});

  let creds = {
    email: "test@example.com",
    sessionToken: "sessionToken",
    kA: "the kA value",
    kB: "the kB value",
    verified: true
  };

  // tell the storage that the MP is locked.
  fxa.internal.signedInUserStorage.__defineGetter__("_isLoggedIn", function() false);
  yield fxa.setSignedInUser(creds);

  // This should have stored stuff in the .json, and the login manager stuff
  // will not exist.
  let path = OS.Path.join(OS.Constants.Path.profileDir, "signedInUser.json");
  let data = yield CommonUtils.readJSON(path);

  Assert.strictEqual(data.accountData.email, creds.email, "correct email in the clear text");
  Assert.strictEqual(data.accountData.sessionToken, creds.sessionToken, "correct sessionToken in the clear text");
  Assert.strictEqual(data.accountData.verified, creds.verified, "correct verified flag");

  Assert.ok(!("kA" in data.accountData), "kA not stored in clear text");
  Assert.ok(!("kB" in data.accountData), "kB not stored in clear text");

  Assert.strictEqual(getLoginMgrData(), null, "login mgr data doesn't exist");
  yield fxa.signOut(/* localOnly = */ true)
});

add_task(function test_consistentWithMPEdgeCases() {
  let fxa = new FxAccounts({});

  let creds1 = {
    email: "test@example.com",
    sessionToken: "sessionToken",
    kA: "the kA value",
    kB: "the kB value",
    verified: true
  };

  let creds2 = {
    email: "test2@example.com",
    sessionToken: "sessionToken2",
    kA: "the kA value2",
    kB: "the kB value2",
    verified: false,
  };

  // Log a user in while MP is unlocked.
  yield fxa.setSignedInUser(creds1);

  // tell the storage that the MP is locked - this will prevent logout from
  // being able to clear the data.
  fxa.internal.signedInUserStorage.__defineGetter__("_isLoggedIn", function() false);

  // now set the second credentials.
  yield fxa.setSignedInUser(creds2);

  // We should still have creds1 data in the login manager.
  let login = getLoginMgrData();
  Assert.strictEqual(login.username, creds1.email);
  // and that we do have the first kA in the login manager.
  Assert.strictEqual(JSON.parse(login.password).accountData.kA, creds1.kA,
                     "stale data still in login mgr");

  // Make a new FxA instance (otherwise the values in memory will be used.)
  // Because we haven't overridden _isLoggedIn for this new instance it will
  // treat the MP as unlocked.
  let fxa = new FxAccounts({});

  let accountData = yield fxa.getSignedInUser();
  Assert.strictEqual(accountData.email, creds2.email);
  // we should have no kA at all.
  Assert.strictEqual(accountData.kA, undefined, "stale kA wasn't used");
  yield fxa.signOut(/* localOnly = */ true)
});

add_task(function test_migration() {
  // manually write out the full creds data to the JSON - this will look like
  // old data that needs migration.
  let creds = {
    email: "test@example.com",
    sessionToken: "sessionToken",
    kA: "the kA value",
    kB: "the kB value",
    verified: true
  };
  let toWrite = {
    version: 1,
    accountData: creds,
  };

  let path = OS.Path.join(OS.Constants.Path.profileDir, "signedInUser.json");
  let data = yield CommonUtils.writeJSON(toWrite, path);

  // Create an FxA object - and tell it to load the data.
  let fxa = new FxAccounts({});
  data = yield fxa.getSignedInUser();

  Assert.deepEqual(data, creds, "we should have everything available");

  // now sniff the data on disk - it should have been magically migrated.
  data = yield CommonUtils.readJSON(path);

  Assert.strictEqual(data.accountData.email, creds.email, "correct email in the clear text");
  Assert.strictEqual(data.accountData.sessionToken, creds.sessionToken, "correct sessionToken in the clear text");
  Assert.strictEqual(data.accountData.verified, creds.verified, "correct verified flag");

  Assert.ok(!("kA" in data.accountData), "kA not stored in clear text");
  Assert.ok(!("kB" in data.accountData), "kB not stored in clear text");

  // and it should magically be in the login manager.
  let login = getLoginMgrData();
  Assert.strictEqual(login.username, creds.email);
  // and that we do have the first kA in the login manager.
  Assert.strictEqual(JSON.parse(login.password).accountData.kA, creds.kA,
                     "kA was migrated");

  yield fxa.signOut(/* localOnly = */ true)
});
