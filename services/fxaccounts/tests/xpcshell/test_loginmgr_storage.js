/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for FxAccounts, storage and the master password.

// Stop us hitting the real auth server.
Services.prefs.setCharPref("identity.fxaccounts.auth.uri", "http://localhost");
// See verbose logging from FxAccounts.jsm
Services.prefs.setCharPref("identity.fxaccounts.loglevel", "Trace");

Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/FxAccountsCommon.js");

// Use a backstage pass to get at our LoginManagerStorage object, so we can
// mock the prototype.
var {LoginManagerStorage} = Cu.import("resource://gre/modules/FxAccountsStorage.jsm", {});
var isLoggedIn = true;
LoginManagerStorage.prototype.__defineGetter__("_isLoggedIn", () => isLoggedIn);

function setLoginMgrLoggedInState(loggedIn) {
  isLoggedIn = loggedIn;
}


initTestLogging("Trace");

function getLoginMgrData() {
  let logins = Services.logins.findLogins({}, FXA_PWDMGR_HOST, null, FXA_PWDMGR_REALM);
  if (logins.length == 0) {
    return null;
  }
  Assert.equal(logins.length, 1, "only 1 login available");
  return logins[0];
}

function createFxAccounts() {
  return new FxAccounts({
    _getDeviceName() {
      return "mock device name";
    },
    fxaPushService: {
      registerPushEndpoint() {
        return new Promise((resolve) => {
          resolve({
            endpoint: "http://mochi.test:8888"
          });
        });
      },
    }
  });
}

add_task(async function test_simple() {
  let fxa = createFxAccounts();

  let creds = {
    uid: "abcd",
    email: "test@example.com",
    sessionToken: "sessionToken",
    kSync: "the kSync value",
    kXCS: "the kXCS value",
    kExtSync: "the kExtSync value",
    kExtKbHash: "the kExtKbHash value",
    verified: true
  };
  await fxa.setSignedInUser(creds);

  // This should have stored stuff in both the .json file in the profile
  // dir, and the login dir.
  let path = OS.Path.join(OS.Constants.Path.profileDir, "signedInUser.json");
  let data = await CommonUtils.readJSON(path);

  Assert.strictEqual(data.accountData.email, creds.email, "correct email in the clear text");
  Assert.strictEqual(data.accountData.sessionToken, creds.sessionToken, "correct sessionToken in the clear text");
  Assert.strictEqual(data.accountData.verified, creds.verified, "correct verified flag");

  Assert.ok(!("kSync" in data.accountData), "kSync not stored in clear text");
  Assert.ok(!("kXCS" in data.accountData), "kXCS not stored in clear text");
  Assert.ok(!("kExtSync" in data.accountData), "kExtSync not stored in clear text");
  Assert.ok(!("kExtKbHash" in data.accountData), "kExtKbHash not stored in clear text");

  let login = getLoginMgrData();
  Assert.strictEqual(login.username, creds.uid, "uid used for username");
  let loginData = JSON.parse(login.password);
  Assert.strictEqual(loginData.version, data.version, "same version flag in both places");
  Assert.strictEqual(loginData.accountData.kSync, creds.kSync, "correct kSync in the login mgr");
  Assert.strictEqual(loginData.accountData.kXCS, creds.kXCS, "correct kXCS in the login mgr");
  Assert.strictEqual(loginData.accountData.kExtSync, creds.kExtSync, "correct kExtSync in the login mgr");
  Assert.strictEqual(loginData.accountData.kExtKbHash, creds.kExtKbHash, "correct kExtKbHash in the login mgr");

  Assert.ok(!("email" in loginData), "email not stored in the login mgr json");
  Assert.ok(!("sessionToken" in loginData), "sessionToken not stored in the login mgr json");
  Assert.ok(!("verified" in loginData), "verified not stored in the login mgr json");

  await fxa.signOut(/* localOnly = */ true);
  Assert.strictEqual(getLoginMgrData(), null, "login mgr data deleted on logout");
});

add_task(async function test_MPLocked() {
  let fxa = createFxAccounts();

  let creds = {
    uid: "abcd",
    email: "test@example.com",
    sessionToken: "sessionToken",
    kSync: "the kSync value",
    kXCS: "the kXCS value",
    kExtSync: "the kExtSync value",
    kExtKbHash: "the kExtKbHash value",
    verified: true
  };

  Assert.strictEqual(getLoginMgrData(), null, "no login mgr at the start");
  // tell the storage that the MP is locked.
  setLoginMgrLoggedInState(false);
  await fxa.setSignedInUser(creds);

  // This should have stored stuff in the .json, and the login manager stuff
  // will not exist.
  let path = OS.Path.join(OS.Constants.Path.profileDir, "signedInUser.json");
  let data = await CommonUtils.readJSON(path);

  Assert.strictEqual(data.accountData.email, creds.email, "correct email in the clear text");
  Assert.strictEqual(data.accountData.sessionToken, creds.sessionToken, "correct sessionToken in the clear text");
  Assert.strictEqual(data.accountData.verified, creds.verified, "correct verified flag");

  Assert.ok(!("kSync" in data.accountData), "kSync not stored in clear text");
  Assert.ok(!("kXCS" in data.accountData), "kXCS not stored in clear text");
  Assert.ok(!("kExtSync" in data.accountData), "kExtSync not stored in clear text");
  Assert.ok(!("kExtKbHash" in data.accountData), "kExtKbHash not stored in clear text");

  Assert.strictEqual(getLoginMgrData(), null, "login mgr data doesn't exist");
  await fxa.signOut(/* localOnly = */ true);
});


add_task(async function test_consistentWithMPEdgeCases() {
  setLoginMgrLoggedInState(true);

  let fxa = createFxAccounts();

  let creds1 = {
    uid: "uid1",
    email: "test@example.com",
    sessionToken: "sessionToken",
    kSync: "the kSync value",
    kXCS: "the kXCS value",
    kExtSync: "the kExtSync value",
    kExtKbHash: "the kExtKbHash value",
    verified: true
  };

  let creds2 = {
    uid: "uid2",
    email: "test2@example.com",
    sessionToken: "sessionToken2",
    kSync: "the kSync value2",
    kXCS: "the kXCS value2",
    kExtSync: "the kExtSync value2",
    kExtKbHash: "the kExtKbHash value2",
    verified: false,
  };

  // Log a user in while MP is unlocked.
  await fxa.setSignedInUser(creds1);

  // tell the storage that the MP is locked - this will prevent logout from
  // being able to clear the data.
  setLoginMgrLoggedInState(false);

  // now set the second credentials.
  await fxa.setSignedInUser(creds2);

  // We should still have creds1 data in the login manager.
  let login = getLoginMgrData();
  Assert.strictEqual(login.username, creds1.uid);
  // and that we do have the first kSync in the login manager.
  Assert.strictEqual(JSON.parse(login.password).accountData.kSync, creds1.kSync,
                     "stale data still in login mgr");

  // Make a new FxA instance (otherwise the values in memory will be used)
  // and we want the login manager to be unlocked.
  setLoginMgrLoggedInState(true);
  fxa = createFxAccounts();

  let accountData = await fxa.getSignedInUser();
  Assert.strictEqual(accountData.email, creds2.email);
  // we should have no kSync at all.
  Assert.strictEqual(accountData.kSync, undefined, "stale kSync wasn't used");
  await fxa.signOut(/* localOnly = */ true);
});

// A test for the fact we will accept either a UID or email when looking in
// the login manager.
add_task(async function test_uidMigration() {
  setLoginMgrLoggedInState(true);
  Assert.strictEqual(getLoginMgrData(), null, "expect no logins at the start");

  // create the login entry using email as a key.
  let contents = {kSync: "kSync"};

  let loginInfo = new Components.Constructor(
   "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
  let login = new loginInfo(FXA_PWDMGR_HOST,
                            null, // aFormSubmitURL,
                            FXA_PWDMGR_REALM, // aHttpRealm,
                            "foo@bar.com", // aUsername
                            JSON.stringify(contents), // aPassword
                            "", // aUsernameField
                            "");// aPasswordField
  Services.logins.addLogin(login);

  // ensure we read it.
  let storage = new LoginManagerStorage();
  let got = await storage.get("uid", "foo@bar.com");
  Assert.deepEqual(got, contents);
});
