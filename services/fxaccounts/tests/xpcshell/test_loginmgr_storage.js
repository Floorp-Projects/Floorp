/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for FxAccounts, storage and the master password.

// See verbose logging from FxAccounts.jsm
Services.prefs.setCharPref("identity.fxaccounts.loglevel", "Trace");

const { FxAccounts } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccounts.sys.mjs"
);
const { FXA_PWDMGR_HOST, FXA_PWDMGR_REALM } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsCommon.sys.mjs"
);

// Use a backstage pass to get at our LoginManagerStorage object, so we can
// mock the prototype.
var { LoginManagerStorage } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsStorage.sys.mjs"
);
var isLoggedIn = true;
LoginManagerStorage.prototype.__defineGetter__("_isLoggedIn", () => isLoggedIn);

function setLoginMgrLoggedInState(loggedIn) {
  isLoggedIn = loggedIn;
}

initTestLogging("Trace");

async function getLoginMgrData() {
  let logins = await Services.logins.searchLoginsAsync({
    origin: FXA_PWDMGR_HOST,
    httpRealm: FXA_PWDMGR_REALM,
  });
  if (!logins.length) {
    return null;
  }
  Assert.equal(logins.length, 1, "only 1 login available");
  return logins[0];
}

function createFxAccounts() {
  return new FxAccounts({
    _fxAccountsClient: {
      async registerDevice() {
        return { id: "deviceAAAAAA" };
      },
      async recoveryEmailStatus() {
        return { verified: true };
      },
      async signOut() {},
    },
    updateDeviceRegistration() {},
    _getDeviceName() {
      return "mock device name";
    },
    observerPreloads: [],
    fxaPushService: {
      async registerPushEndpoint() {
        return {
          endpoint: "http://mochi.test:8888",
          getKey() {
            return null;
          },
        };
      },
      async unsubscribe() {
        return true;
      },
    },
  });
}

add_task(async function test_simple() {
  let fxa = createFxAccounts();

  let creds = {
    uid: "abcd",
    email: "test@example.com",
    sessionToken: "sessionToken",
    scopedKeys: {
      ...MOCK_ACCOUNT_KEYS.scopedKeys,
    },
    verified: true,
  };
  await fxa._internal.setSignedInUser(creds);

  // This should have stored stuff in both the .json file in the profile
  // dir, and the login dir.
  let path = PathUtils.join(PathUtils.profileDir, "signedInUser.json");
  let data = await IOUtils.readJSON(path);

  Assert.strictEqual(
    data.accountData.email,
    creds.email,
    "correct email in the clear text"
  );
  Assert.strictEqual(
    data.accountData.sessionToken,
    creds.sessionToken,
    "correct sessionToken in the clear text"
  );
  Assert.strictEqual(
    data.accountData.verified,
    creds.verified,
    "correct verified flag"
  );

  Assert.ok(
    !("scopedKeys" in data.accountData),
    "scopedKeys not stored in clear text"
  );

  let login = await getLoginMgrData();
  Assert.strictEqual(login.username, creds.uid, "uid used for username");
  let loginData = JSON.parse(login.password);
  Assert.strictEqual(
    loginData.version,
    data.version,
    "same version flag in both places"
  );
  Assert.deepEqual(
    loginData.accountData.scopedKeys,
    creds.scopedKeys,
    "correct scoped keys in the login mgr"
  );
  Assert.ok(!("email" in loginData), "email not stored in the login mgr json");
  Assert.ok(
    !("sessionToken" in loginData),
    "sessionToken not stored in the login mgr json"
  );
  Assert.ok(
    !("verified" in loginData),
    "verified not stored in the login mgr json"
  );

  await fxa.signOut(/* localOnly = */ true);
  Assert.strictEqual(
    await getLoginMgrData(),
    null,
    "login mgr data deleted on logout"
  );
});

add_task(async function test_MPLocked() {
  let fxa = createFxAccounts();

  let creds = {
    uid: "abcd",
    email: "test@example.com",
    sessionToken: "sessionToken",
    scopedKeys: {
      ...MOCK_ACCOUNT_KEYS.scopedKeys,
    },
    verified: true,
  };

  Assert.strictEqual(
    await getLoginMgrData(),
    null,
    "no login mgr at the start"
  );
  // tell the storage that the MP is locked.
  setLoginMgrLoggedInState(false);
  await fxa._internal.setSignedInUser(creds);

  // This should have stored stuff in the .json, and the login manager stuff
  // will not exist.
  let path = PathUtils.join(PathUtils.profileDir, "signedInUser.json");
  let data = await IOUtils.readJSON(path);

  Assert.strictEqual(
    data.accountData.email,
    creds.email,
    "correct email in the clear text"
  );
  Assert.strictEqual(
    data.accountData.sessionToken,
    creds.sessionToken,
    "correct sessionToken in the clear text"
  );
  Assert.strictEqual(
    data.accountData.verified,
    creds.verified,
    "correct verified flag"
  );

  Assert.ok(
    !("scopedKeys" in data.accountData),
    "scopedKeys not stored in clear text"
  );

  Assert.strictEqual(
    await getLoginMgrData(),
    null,
    "login mgr data doesn't exist"
  );
  await fxa.signOut(/* localOnly = */ true);
});

add_task(async function test_consistentWithMPEdgeCases() {
  setLoginMgrLoggedInState(true);

  let fxa = createFxAccounts();

  let creds1 = {
    uid: "uid1",
    email: "test@example.com",
    sessionToken: "sessionToken",
    scopedKeys: {
      [SCOPE_OLD_SYNC]: {
        kid: "key id 1",
        k: "key material 1",
        kty: "oct",
      },
    },
    verified: true,
  };

  let creds2 = {
    uid: "uid2",
    email: "test2@example.com",
    sessionToken: "sessionToken2",
    [SCOPE_OLD_SYNC]: {
      kid: "key id 2",
      k: "key material 2",
      kty: "oct",
    },
    verified: false,
  };

  // Log a user in while MP is unlocked.
  await fxa._internal.setSignedInUser(creds1);

  // tell the storage that the MP is locked - this will prevent logout from
  // being able to clear the data.
  setLoginMgrLoggedInState(false);

  // now set the second credentials.
  await fxa._internal.setSignedInUser(creds2);

  // We should still have creds1 data in the login manager.
  let login = await getLoginMgrData();
  Assert.strictEqual(login.username, creds1.uid);
  // and that we do have the first scopedKeys in the login manager.
  Assert.deepEqual(
    JSON.parse(login.password).accountData.scopedKeys,
    creds1.scopedKeys,
    "stale data still in login mgr"
  );

  // Make a new FxA instance (otherwise the values in memory will be used)
  // and we want the login manager to be unlocked.
  setLoginMgrLoggedInState(true);
  fxa = createFxAccounts();

  let accountData = await fxa.getSignedInUser();
  Assert.strictEqual(accountData.email, creds2.email);
  // we should have no scopedKeys at all.
  Assert.strictEqual(
    accountData.scopedKeys,
    undefined,
    "stale scopedKey wasn't used"
  );
  await fxa.signOut(/* localOnly = */ true);
});

// A test for the fact we will accept either a UID or email when looking in
// the login manager.
add_task(async function test_uidMigration() {
  setLoginMgrLoggedInState(true);
  Assert.strictEqual(
    await getLoginMgrData(),
    null,
    "expect no logins at the start"
  );

  // create the login entry using email as a key.
  let contents = {
    scopedKeys: {
      ...MOCK_ACCOUNT_KEYS.scopedKeys,
    },
  };

  let loginInfo = new Components.Constructor(
    "@mozilla.org/login-manager/loginInfo;1",
    Ci.nsILoginInfo,
    "init"
  );
  let login = new loginInfo(
    FXA_PWDMGR_HOST,
    null, // aFormActionOrigin,
    FXA_PWDMGR_REALM, // aHttpRealm,
    "foo@bar.com", // aUsername
    JSON.stringify(contents), // aPassword
    "", // aUsernameField
    ""
  ); // aPasswordField
  await Services.logins.addLoginAsync(login);

  // ensure we read it.
  let storage = new LoginManagerStorage();
  let got = await storage.get("uid", "foo@bar.com");
  Assert.deepEqual(got, contents);
});
