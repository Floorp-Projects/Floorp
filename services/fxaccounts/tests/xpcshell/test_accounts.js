/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CryptoUtils } = ChromeUtils.import(
  "resource://services-crypto/utils.js"
);
const { FxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);
const { FxAccountsClient } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsClient.jsm"
);
const {
  ERRNO_INVALID_AUTH_TOKEN,
  ERROR_NETWORK,
  ERROR_NO_ACCOUNT,
  FX_OAUTH_CLIENT_ID,
  ONLOGIN_NOTIFICATION,
  ONLOGOUT_NOTIFICATION,
  ONVERIFIED_NOTIFICATION,
  DEPRECATED_SCOPE_ECOSYSTEM_TELEMETRY,
  PREF_LAST_FXA_USER,
} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);

// We grab some additional stuff via backstage passes.
var { AccountState } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm",
  null
);

const ONE_HOUR_MS = 1000 * 60 * 60;
const ONE_DAY_MS = ONE_HOUR_MS * 24;
const MOCK_TOKEN_RESPONSE = {
  access_token:
    "43793fdfffec22eb39fc3c44ed09193a6fde4c24e5d6a73f73178597b268af69",
  token_type: "bearer",
  scope: "https://identity.mozilla.com/apps/oldsync",
  expires_in: 21600,
  auth_at: 1589579900,
};

initTestLogging("Trace");

var log = Log.repository.getLogger("Services.FxAccounts.test");
log.level = Log.Level.Debug;

// See verbose logging from FxAccounts.jsm and jwcrypto.jsm.
Services.prefs.setCharPref("identity.fxaccounts.loglevel", "Trace");
Log.repository.getLogger("FirefoxAccounts").level = Log.Level.Trace;
Services.prefs.setCharPref("services.crypto.jwcrypto.log.level", "Debug");

/*
 * The FxAccountsClient communicates with the remote Firefox
 * Accounts auth server.  Mock the server calls, with a little
 * lag time to simulate some latency.
 *
 * We add the _verified attribute to mock the change in verification
 * state on the FXA server.
 */

function MockStorageManager() {}

MockStorageManager.prototype = {
  promiseInitialized: Promise.resolve(),

  initialize(accountData) {
    this.accountData = accountData;
  },

  finalize() {
    return Promise.resolve();
  },

  getAccountData(fields = null) {
    let result;
    if (!this.accountData) {
      result = null;
    } else if (fields == null) {
      // can't use cloneInto as the keys get upset...
      result = {};
      for (let field of Object.keys(this.accountData)) {
        result[field] = this.accountData[field];
      }
    } else {
      if (!Array.isArray(fields)) {
        fields = [fields];
      }
      result = {};
      for (let field of fields) {
        result[field] = this.accountData[field];
      }
    }
    return Promise.resolve(result);
  },

  updateAccountData(updatedFields) {
    if (!this.accountData) {
      return Promise.resolve();
    }
    for (let [name, value] of Object.entries(updatedFields)) {
      if (value == null) {
        delete this.accountData[name];
      } else {
        this.accountData[name] = value;
      }
    }
    return Promise.resolve();
  },

  deleteAccountData() {
    this.accountData = null;
    return Promise.resolve();
  },
};

function MockFxAccountsClient() {
  this._email = "nobody@example.com";
  this._verified = false;
  this._deletedOnServer = false; // for our accountStatus mock

  // mock calls up to the auth server to determine whether the
  // user account has been verified
  this.recoveryEmailStatus = async function(sessionToken) {
    // simulate a call to /recovery_email/status
    return {
      email: this._email,
      verified: this._verified,
    };
  };

  this.accountStatus = async function(uid) {
    return !!uid && !this._deletedOnServer;
  };

  this.sessionStatus = async function() {
    // If the sessionStatus check says an account is OK, we typically will not
    // end up calling accountStatus - so this must return false if accountStatus
    // would.
    return !this._deletedOnServer;
  };

  this.accountKeys = function(keyFetchToken) {
    return new Promise(resolve => {
      do_timeout(50, () => {
        resolve({
          kA: expandBytes("11"),
          wrapKB: expandBytes("22"),
        });
      });
    });
  };

  this.getScopedKeyData = function(sessionToken, client_id, scopes) {
    Assert.ok(sessionToken);
    Assert.equal(client_id, FX_OAUTH_CLIENT_ID);
    Assert.equal(scopes, SCOPE_OLD_SYNC);
    return new Promise(resolve => {
      do_timeout(50, () => {
        resolve({
          "https://identity.mozilla.com/apps/oldsync": {
            identifier: "https://identity.mozilla.com/apps/oldsync",
            keyRotationSecret:
              "0000000000000000000000000000000000000000000000000000000000000000",
            keyRotationTimestamp: 1234567890123,
          },
        });
      });
    });
  };

  this.resendVerificationEmail = function(sessionToken) {
    // Return the session token to show that we received it in the first place
    return Promise.resolve(sessionToken);
  };

  this.signOut = () => Promise.resolve();

  FxAccountsClient.apply(this);
}
MockFxAccountsClient.prototype = {
  __proto__: FxAccountsClient.prototype,
};

/*
 * We need to mock the FxAccounts module's interfaces to external
 * services, such as storage and the FxAccounts client.  We also
 * mock the now() method, so that we can simulate the passing of
 * time and verify that signatures expire correctly.
 */
function MockFxAccounts(credentials = null) {
  let result = new FxAccounts({
    VERIFICATION_POLL_TIMEOUT_INITIAL: 100, // 100ms

    _getCertificateSigned_calls: [],
    _d_signCertificate: PromiseUtils.defer(),
    _now_is: new Date(),
    now() {
      return this._now_is;
    },
    newAccountState(newCredentials) {
      // we use a real accountState but mocked storage.
      let storage = new MockStorageManager();
      storage.initialize(newCredentials);
      return new AccountState(storage);
    },
    fxAccountsClient: new MockFxAccountsClient(),
    observerPreloads: [],
    device: {
      _registerOrUpdateDevice() {},
    },
    profile: {
      getProfile() {
        return null;
      },
    },
  });
  // and for convenience so we don't have to touch as many lines in this test
  // when we refactored FxAccounts.jsm :)
  result.setSignedInUser = function(creds) {
    return result._internal.setSignedInUser(creds);
  };
  return result;
}

/*
 * Some tests want a "real" fxa instance - however, we still mock the storage
 * to keep the tests fast on b2g.
 */
async function MakeFxAccounts({ internal = {}, credentials } = {}) {
  if (!internal.newAccountState) {
    // we use a real accountState but mocked storage.
    internal.newAccountState = function(newCredentials) {
      let storage = new MockStorageManager();
      storage.initialize(newCredentials);
      return new AccountState(storage);
    };
  }
  if (!internal._signOutServer) {
    internal._signOutServer = () => Promise.resolve();
  }
  if (internal.device) {
    if (!internal.device._registerOrUpdateDevice) {
      internal.device._registerOrUpdateDevice = () => Promise.resolve();
    }
  } else {
    internal.device = {
      _registerOrUpdateDevice() {},
    };
  }
  if (!internal.observerPreloads) {
    internal.observerPreloads = [];
  }
  let result = new FxAccounts(internal);

  if (credentials) {
    await result._internal.setSignedInUser(credentials);
  }
  return result;
}

add_task(async function test_get_signed_in_user_initially_unset() {
  _("Check getSignedInUser initially and after signout reports no user");
  let account = await MakeFxAccounts();
  let credentials = {
    email: "foo@example.com",
    uid: "1234567890abcdef1234567890abcdef",
    sessionToken: "dead",
    verified: true,
    ...MOCK_ACCOUNT_KEYS,
  };
  let result = await account.getSignedInUser();
  Assert.equal(result, null);

  await account._internal.setSignedInUser(credentials);

  // getSignedInUser only returns a subset.
  result = await account.getSignedInUser();
  Assert.deepEqual(result.email, credentials.email);
  Assert.deepEqual(result.scopedKeys, undefined);
  Assert.deepEqual(result.kSync, undefined);
  Assert.deepEqual(result.kXCS, undefined);
  Assert.deepEqual(result.kExtSync, undefined);
  Assert.deepEqual(result.kExtKbHash, undefined);
  // for the sake of testing, use the low-level function to check it's all there
  result = await account._internal.currentAccountState.getUserAccountData();
  Assert.deepEqual(result.email, credentials.email);
  Assert.deepEqual(result.scopedKeys, credentials.scopedKeys);
  Assert.ok(result.kSync);
  Assert.ok(result.kXCS);
  Assert.ok(result.kExtSync);
  Assert.ok(result.kExtKbHash);

  // sign out
  let localOnly = true;
  await account.signOut(localOnly);

  // user should be undefined after sign out
  result = await account.getSignedInUser();
  Assert.equal(result, null);
});

add_task(async function test_set_signed_in_user_signs_out_previous_account() {
  _("Check setSignedInUser signs out the previous account.");
  let signOutServerCalled = false;
  let credentials = {
    email: "foo@example.com",
    uid: "1234567890abcdef1234567890abcdef",
    sessionToken: "dead",
    verified: true,
    ...MOCK_ACCOUNT_KEYS,
  };
  let account = await MakeFxAccounts({ credentials });

  account._internal._signOutServer = () => {
    signOutServerCalled = true;
    return Promise.resolve(true);
  };

  await account._internal.setSignedInUser(credentials);
  Assert.ok(signOutServerCalled);
});

add_task(async function test_update_account_data() {
  _("Check updateUserAccountData does the right thing.");
  let credentials = {
    email: "foo@example.com",
    uid: "1234567890abcdef1234567890abcdef",
    sessionToken: "dead",
    verified: true,
    ...MOCK_ACCOUNT_KEYS,
  };
  let account = await MakeFxAccounts({ credentials });

  let newCreds = {
    email: credentials.email,
    uid: credentials.uid,
    sessionToken: "alive",
  };
  await account._internal.updateUserAccountData(newCreds);
  Assert.equal(
    (await account._internal.getUserAccountData()).sessionToken,
    "alive",
    "new field value was saved"
  );

  // but we should fail attempting to change the uid.
  newCreds = {
    email: credentials.email,
    uid: "11111111111111111111222222222222",
    sessionToken: "alive",
  };
  await Assert.rejects(
    account._internal.updateUserAccountData(newCreds),
    /The specified credentials aren't for the current user/
  );

  // should fail without the uid.
  newCreds = {
    sessionToken: "alive",
  };
  await Assert.rejects(
    account._internal.updateUserAccountData(newCreds),
    /The specified credentials have no uid/
  );

  // and should fail with a field name that's not known by storage.
  newCreds = {
    email: credentials.email,
    uid: "11111111111111111111222222222222",
    foo: "bar",
  };
  await Assert.rejects(
    account._internal.updateUserAccountData(newCreds),
    /The specified credentials aren't for the current user/
  );
});

// Sanity-check that our mocked client is working correctly
add_test(function test_client_mock() {
  let fxa = new MockFxAccounts();
  let client = fxa._internal.fxAccountsClient;
  Assert.equal(client._verified, false);
  Assert.equal(typeof client.signIn, "function");

  // The recoveryEmailStatus function eventually fulfills its promise
  client.recoveryEmailStatus().then(response => {
    Assert.equal(response.verified, false);
    run_next_test();
  });
});

// Sign in a user, and after a little while, verify the user's email.
// Right after signing in the user, we should get the 'onlogin' notification.
// Polling should detect that the email is verified, and eventually
// 'onverified' should be observed
add_test(function test_verification_poll() {
  let fxa = new MockFxAccounts();
  let test_user = getTestUser("francine");
  let login_notification_received = false;

  makeObserver(ONVERIFIED_NOTIFICATION, function() {
    log.debug("test_verification_poll observed onverified");
    // Once email verification is complete, we will observe onverified
    fxa._internal
      .getUserAccountData()
      .then(user => {
        // And confirm that the user's state has changed
        Assert.equal(user.verified, true);
        Assert.equal(user.email, test_user.email);
        Assert.equal(
          Services.prefs.getStringPref(PREF_LAST_FXA_USER),
          CryptoUtils.sha256Base64(test_user.email)
        );
        Assert.ok(login_notification_received);
      })
      .finally(run_next_test);
  });

  makeObserver(ONLOGIN_NOTIFICATION, function() {
    log.debug("test_verification_poll observer onlogin");
    login_notification_received = true;
  });

  fxa.setSignedInUser(test_user).then(() => {
    fxa._internal.getUserAccountData().then(user => {
      // The user is signing in, but email has not been verified yet
      Assert.equal(user.verified, false);
      do_timeout(200, function() {
        log.debug("Mocking verification of francine's email");
        fxa._internal.fxAccountsClient._email = test_user.email;
        fxa._internal.fxAccountsClient._verified = true;
      });
    });
  });
});

// Sign in the user, but never verify the email.  The check-email
// poll should time out.  No verifiedlogin event should be observed, and the
// internal whenVerified promise should be rejected
add_test(function test_polling_timeout() {
  // This test could be better - the onverified observer might fire on
  // somebody else's stack, and we're not making sure that we're not receiving
  // such a message. In other words, this tests either failure, or success, but
  // not both.

  let fxa = new MockFxAccounts();
  let test_user = getTestUser("carol");

  let removeObserver = makeObserver(ONVERIFIED_NOTIFICATION, function() {
    do_throw("We should not be getting a login event!");
  });

  fxa._internal.POLL_SESSION = 1;

  let p = fxa._internal.whenVerified({});

  fxa.setSignedInUser(test_user).then(() => {
    p.then(
      success => {
        do_throw("this should not succeed");
      },
      fail => {
        removeObserver();
        fxa.signOut().then(run_next_test);
      }
    );
  });
});

// For bug 1585299 - ensure we only get a single ONVERIFIED notification.
add_task(async function test_onverified_once() {
  let fxa = new MockFxAccounts();
  let user = getTestUser("francine");

  let numNotifications = 0;

  function observe(aSubject, aTopic, aData) {
    numNotifications += 1;
  }
  Services.obs.addObserver(observe, ONVERIFIED_NOTIFICATION);

  fxa._internal.POLL_SESSION = 1;

  await fxa.setSignedInUser(user);

  Assert.ok(!(await fxa.getSignedInUser()).verified, "starts unverified");

  await fxa._internal.startPollEmailStatus(
    fxa._internal.currentAccountState,
    user.sessionToken,
    "start"
  );

  Assert.ok(!(await fxa.getSignedInUser()).verified, "still unverified");

  log.debug("Mocking verification of francine's email");
  fxa._internal.fxAccountsClient._email = user.email;
  fxa._internal.fxAccountsClient._verified = true;

  await fxa._internal.startPollEmailStatus(
    fxa._internal.currentAccountState,
    user.sessionToken,
    "again"
  );

  Assert.ok((await fxa.getSignedInUser()).verified, "now verified");

  Assert.equal(numNotifications, 1, "expect exactly 1 ONVERIFIED");

  Services.obs.removeObserver(observe, ONVERIFIED_NOTIFICATION);
  await fxa.signOut();
});

add_test(function test_pollEmailStatus_start_verified() {
  let fxa = new MockFxAccounts();
  let test_user = getTestUser("carol");

  fxa._internal.POLL_SESSION = 20 * 60000;
  fxa._internal.VERIFICATION_POLL_TIMEOUT_INITIAL = 50000;

  fxa.setSignedInUser(test_user).then(() => {
    fxa._internal.getUserAccountData().then(user => {
      fxa._internal.fxAccountsClient._email = test_user.email;
      fxa._internal.fxAccountsClient._verified = true;
      const mock = sinon.mock(fxa._internal);
      mock.expects("_scheduleNextPollEmailStatus").never();
      fxa._internal
        .startPollEmailStatus(
          fxa._internal.currentAccountState,
          user.sessionToken,
          "start"
        )
        .then(() => {
          mock.verify();
          mock.restore();
          run_next_test();
        });
    });
  });
});

add_test(function test_pollEmailStatus_start() {
  let fxa = new MockFxAccounts();
  let test_user = getTestUser("carol");

  fxa._internal.POLL_SESSION = 20 * 60000;
  fxa._internal.VERIFICATION_POLL_TIMEOUT_INITIAL = 123456;

  fxa.setSignedInUser(test_user).then(() => {
    fxa._internal.getUserAccountData().then(user => {
      const mock = sinon.mock(fxa._internal);
      mock
        .expects("_scheduleNextPollEmailStatus")
        .once()
        .withArgs(
          fxa._internal.currentAccountState,
          user.sessionToken,
          123456,
          "start"
        );
      fxa._internal
        .startPollEmailStatus(
          fxa._internal.currentAccountState,
          user.sessionToken,
          "start"
        )
        .then(() => {
          mock.verify();
          mock.restore();
          run_next_test();
        });
    });
  });
});

add_test(function test_pollEmailStatus_start_subsequent() {
  let fxa = new MockFxAccounts();
  let test_user = getTestUser("carol");

  fxa._internal.POLL_SESSION = 20 * 60000;
  fxa._internal.VERIFICATION_POLL_TIMEOUT_INITIAL = 123456;
  fxa._internal.VERIFICATION_POLL_TIMEOUT_SUBSEQUENT = 654321;
  fxa._internal.VERIFICATION_POLL_START_SLOWDOWN_THRESHOLD = -1;

  fxa.setSignedInUser(test_user).then(() => {
    fxa._internal.getUserAccountData().then(user => {
      const mock = sinon.mock(fxa._internal);
      mock
        .expects("_scheduleNextPollEmailStatus")
        .once()
        .withArgs(
          fxa._internal.currentAccountState,
          user.sessionToken,
          654321,
          "start"
        );
      fxa._internal
        .startPollEmailStatus(
          fxa._internal.currentAccountState,
          user.sessionToken,
          "start"
        )
        .then(() => {
          mock.verify();
          mock.restore();
          run_next_test();
        });
    });
  });
});

add_test(function test_pollEmailStatus_browser_startup() {
  let fxa = new MockFxAccounts();
  let test_user = getTestUser("carol");

  fxa._internal.POLL_SESSION = 20 * 60000;
  fxa._internal.VERIFICATION_POLL_TIMEOUT_SUBSEQUENT = 654321;

  fxa.setSignedInUser(test_user).then(() => {
    fxa._internal.getUserAccountData().then(user => {
      const mock = sinon.mock(fxa._internal);
      mock
        .expects("_scheduleNextPollEmailStatus")
        .once()
        .withArgs(
          fxa._internal.currentAccountState,
          user.sessionToken,
          654321,
          "browser-startup"
        );
      fxa._internal
        .startPollEmailStatus(
          fxa._internal.currentAccountState,
          user.sessionToken,
          "browser-startup"
        )
        .then(() => {
          mock.verify();
          mock.restore();
          run_next_test();
        });
    });
  });
});

add_test(function test_pollEmailStatus_push() {
  let fxa = new MockFxAccounts();
  let test_user = getTestUser("carol");

  fxa.setSignedInUser(test_user).then(() => {
    fxa._internal.getUserAccountData().then(user => {
      const mock = sinon.mock(fxa._internal);
      mock.expects("_scheduleNextPollEmailStatus").never();
      fxa._internal
        .startPollEmailStatus(
          fxa._internal.currentAccountState,
          user.sessionToken,
          "push"
        )
        .then(() => {
          mock.verify();
          mock.restore();
          run_next_test();
        });
    });
  });
});

add_test(function test_getKeyForScope() {
  let fxa = new MockFxAccounts();
  let user = getTestUser("eusebius");

  // Once email has been verified, we will be able to get keys
  user.verified = true;

  fxa.setSignedInUser(user).then(() => {
    fxa._internal.getUserAccountData().then(user2 => {
      // Before getKeyForScope, we have no keys
      Assert.equal(!!user2.scopedKeys, false);
      Assert.equal(!!user2.kSync, false);
      Assert.equal(!!user2.kXCS, false);
      Assert.equal(!!user2.kExtSync, false);
      Assert.equal(!!user2.kExtKbHash, false);
      // And we still have a key-fetch token and unwrapBKey to use
      Assert.equal(!!user2.keyFetchToken, true);
      Assert.equal(!!user2.unwrapBKey, true);

      fxa.keys.getKeyForScope(SCOPE_OLD_SYNC).then(() => {
        fxa._internal.getUserAccountData().then(user3 => {
          // Now we should have keys
          Assert.equal(fxa._internal.isUserEmailVerified(user3), true);
          Assert.equal(!!user3.verified, true);
          Assert.notEqual(null, user3.scopedKeys);
          Assert.notEqual(null, user3.kSync);
          Assert.notEqual(null, user3.kXCS);
          Assert.notEqual(null, user3.kExtSync);
          Assert.notEqual(null, user3.kExtKbHash);
          Assert.equal(user3.keyFetchToken, undefined);
          Assert.equal(user3.unwrapBKey, undefined);
          run_next_test();
        });
      });
    });
  });
});

add_task(async function test_getKeyForScope_kb_migration() {
  let fxa = new MockFxAccounts();
  let user = getTestUser("eusebius");

  user.verified = true;
  // Set-up the deprecated set of keys.
  user.kA = "e0245ab7f10e483470388e0a28f0a03379a3b417174fb2b42feab158b4ac2dbd";
  user.kB = "eaf9570b7219a4187d3d6bf3cec2770c2e0719b7cc0dfbb38243d6f1881675e9";

  await fxa.setSignedInUser(user);
  await fxa.keys.getKeyForScope(SCOPE_OLD_SYNC);
  let newUser = await fxa._internal.getUserAccountData();
  Assert.equal(newUser.kA, null);
  Assert.equal(newUser.kB, null);
  Assert.deepEqual(newUser.scopedKeys, {
    "https://identity.mozilla.com/apps/oldsync": {
      kid: "1234567890123-IqQv4onc7VcVE1kTQkyyOw",
      k:
        "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
      kty: "oct",
    },
    "sync:addon_storage": {
      kid: "1234567890123-pBOR6B6JulbJr3BxKVOqIU4Cq_WAjFp4ApLn5NRVARE",
      k:
        "ut7VPrNYfXkA5gTopo2GCr-d4wtclV08TV26Y_Jv2IJlzYWSP26dzRau87gryIA5qJxZ7NnojeCadBjH2U-QyQ",
      kty: "oct",
    },
  });
  // These hex values were manually confirmed to be equivalent to the b64 values above.
  Assert.equal(
    newUser.kSync,
    "0d6fe59791b05fa489e463ea25502e3143f6b7a903aa152e95cd9c6eddbac5b4" +
      "dc68a19097ef65dbd147010ee45222444e66b8b3d7c8a441ebb7dd3dce015a9e"
  );
  Assert.equal(newUser.kXCS, "22a42fe289dced5715135913424cb23b");
  Assert.equal(
    newUser.kExtSync,
    "baded53eb3587d7900e604e8a68d860abf9de30b5c955d3c4d5dba63f26fd882" +
      "65cd85923f6e9dcd16aef3b82bc88039a89c59ecd9e88de09a7418c7d94f90c9"
  );
  Assert.equal(
    newUser.kExtKbHash,
    "a41391e81e89ba56c9af70712953aa214e02abf5808c5a780292e7e4d4550111"
  );
});

add_task(async function test_getKeyForScope_scopedKeys_migration() {
  let fxa = new MockFxAccounts();
  let user = getTestUser("eusebius");

  user.verified = true;
  // Set-up the keys in deprecated fields.
  user.kSync = MOCK_ACCOUNT_KEYS.kSync;
  user.kXCS = MOCK_ACCOUNT_KEYS.kXCS;
  user.kExtSync = MOCK_ACCOUNT_KEYS.kExtSync;
  user.kExtKbHash = MOCK_ACCOUNT_KEYS.kExtKbHash;
  Assert.equal(user.scopedKeys, null);

  await fxa.setSignedInUser(user);
  await fxa.keys.getKeyForScope(SCOPE_OLD_SYNC);
  let newUser = await fxa._internal.getUserAccountData();
  Assert.equal(newUser.kA, null);
  Assert.equal(newUser.kB, null);
  // It should have correctly formatted the corresponding scoped keys.
  const expectedScopedKeys = { ...MOCK_ACCOUNT_KEYS.scopedKeys };
  Assert.deepEqual(newUser.scopedKeys, expectedScopedKeys);
  // And left the existing key fields unchanged.
  Assert.equal(newUser.kSync, user.kSync);
  Assert.equal(newUser.kXCS, user.kXCS);
  Assert.equal(newUser.kExtSync, user.kExtSync);
  Assert.equal(newUser.kExtKbHash, user.kExtKbHash);
});

add_task(
  async function test_getKeyForScope_scopedKeys_migration_removes_deprecated_scoped_keys() {
    let fxa = new MockFxAccounts();
    let user = getTestUser("eusebius");

    const EXTRA_SCOPE = "an unknown, but non-deprecated scope";
    user.verified = true;
    user.ecosystemUserId = "ecoUserId";
    user.ecosystemAnonId = "ecoAnonId";
    user.scopedKeys = {
      ...MOCK_ACCOUNT_KEYS.scopedKeys,
      [DEPRECATED_SCOPE_ECOSYSTEM_TELEMETRY]:
        MOCK_ACCOUNT_KEYS.scopedKeys[SCOPE_OLD_SYNC],
      [EXTRA_SCOPE]: MOCK_ACCOUNT_KEYS.scopedKeys[SCOPE_OLD_SYNC],
    };

    await fxa.setSignedInUser(user);
    await fxa.keys.getKeyForScope(SCOPE_OLD_SYNC);
    let newUser = await fxa._internal.getUserAccountData();
    // It should have removed the deprecated ecosystem_telemetry key,
    // but left the other keys intact.
    const expectedScopedKeys = {
      ...MOCK_ACCOUNT_KEYS.scopedKeys,
      [EXTRA_SCOPE]: MOCK_ACCOUNT_KEYS.scopedKeys[SCOPE_OLD_SYNC],
    };
    Assert.deepEqual(newUser.scopedKeys, expectedScopedKeys);
    Assert.equal(newUser.ecosystemUserId, null);
    Assert.equal(newUser.ecosystemAnonId, null);
  }
);

add_task(async function test_getKeyForScope_nonexistent_account() {
  let fxa = new MockFxAccounts();
  let bismarck = getTestUser("bismarck");

  let client = fxa._internal.fxAccountsClient;
  client.accountStatus = () => Promise.resolve(false);
  client.sessionStatus = () => Promise.resolve(false);
  client.accountKeys = () => {
    return Promise.reject({
      code: 401,
      errno: ERRNO_INVALID_AUTH_TOKEN,
    });
  };

  await fxa.setSignedInUser(bismarck);

  let promiseLogout = new Promise(resolve => {
    makeObserver(ONLOGOUT_NOTIFICATION, function() {
      log.debug("test_getKeyForScope_nonexistent_account observed logout");
      resolve();
    });
  });

  // XXX - the exception message here isn't ideal, but doesn't really matter...
  await Assert.rejects(
    fxa.keys.getKeyForScope(SCOPE_OLD_SYNC),
    /A different user signed in/
  );

  await promiseLogout;

  let user = await fxa._internal.getUserAccountData();
  Assert.equal(user, null);
});

// getKeyForScope with invalid keyFetchToken should delete keyFetchToken from storage
add_task(async function test_getKeyForScope_invalid_token() {
  let fxa = new MockFxAccounts();
  let yusuf = getTestUser("yusuf");

  let client = fxa._internal.fxAccountsClient;
  client.accountStatus = () => Promise.resolve(true); // account exists.
  client.sessionStatus = () => Promise.resolve(false); // session is invalid.
  client.accountKeys = () => {
    return Promise.reject({
      code: 401,
      errno: ERRNO_INVALID_AUTH_TOKEN,
    });
  };

  await fxa.setSignedInUser(yusuf);
  let user = await fxa._internal.getUserAccountData();
  Assert.notEqual(user.encryptedSendTabKeys, null);

  try {
    await fxa.keys.getKeyForScope(SCOPE_OLD_SYNC);
    Assert.ok(false);
  } catch (err) {
    Assert.equal(err.code, 401);
    Assert.equal(err.errno, ERRNO_INVALID_AUTH_TOKEN);
  }

  user = await fxa._internal.getUserAccountData();
  Assert.equal(user.email, yusuf.email);
  Assert.equal(user.keyFetchToken, null);
  // We verify that encryptedSendTabKeys are also wiped
  // when a user's credentials are wiped
  Assert.equal(user.encryptedSendTabKeys, null);
  await fxa._internal.abortExistingFlow();
});

// This is the exact same test vectors as
// https://github.com/mozilla/fxa-crypto-relier/blob/f94f441159029a645a474d4b6439c38308da0bb0/test/deriver/ScopedKeys.js#L58
add_task(async function test_getKeyForScope_oldsync() {
  let fxa = new MockFxAccounts();
  let client = fxa._internal.fxAccountsClient;
  client.getScopedKeyData = () =>
    Promise.resolve({
      "https://identity.mozilla.com/apps/oldsync": {
        identifier: "https://identity.mozilla.com/apps/oldsync",
        keyRotationSecret:
          "0000000000000000000000000000000000000000000000000000000000000000",
        keyRotationTimestamp: 1510726317123,
      },
    });
  let user = {
    ...getTestUser("eusebius"),
    uid: "aeaa1725c7a24ff983c6295725d5fc9b",
    kB: "eaf9570b7219a4187d3d6bf3cec2770c2e0719b7cc0dfbb38243d6f1881675e9",
    verified: true,
  };
  await fxa.setSignedInUser(user);
  const key = await fxa.keys.getKeyForScope(SCOPE_OLD_SYNC);
  Assert.deepEqual(key, {
    scope: SCOPE_OLD_SYNC,
    kid: "1510726317123-IqQv4onc7VcVE1kTQkyyOw",
    k:
      "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
    kty: "oct",
  });
});

add_task(async function test_getScopedKeys_unavailable_scope() {
  let fxa = new MockFxAccounts();
  let user = {
    ...getTestUser("eusebius"),
    uid: "aeaa1725c7a24ff983c6295725d5fc9b",
    verified: true,
    ...MOCK_ACCOUNT_KEYS,
  };
  await fxa.setSignedInUser(user);
  await Assert.rejects(
    fxa.keys.getKeyForScope("otherkeybearingscope"),
    /Key not available for scope/
  );
});

add_task(async function test_getScopedKeys_misconfigured_fxa_server() {
  let fxa = new MockFxAccounts();
  let client = fxa._internal.fxAccountsClient;
  client.getScopedKeyData = () =>
    Promise.resolve({
      wrongscope: {
        identifier: "wrongscope",
        keyRotationSecret:
          "0000000000000000000000000000000000000000000000000000000000000000",
        keyRotationTimestamp: 1510726331712,
      },
    });
  let user = {
    ...getTestUser("eusebius"),
    uid: "aeaa1725c7a24ff983c6295725d5fc9b",
    verified: true,
    kSync:
      "0d6fe59791b05fa489e463ea25502e3143f6b7a903aa152e95cd9c6eddbac5b4dc68a19097ef65dbd147010ee45222444e66b8b3d7c8a441ebb7dd3dce015a9e",
    kXCS: "22a42fe289dced5715135913424cb23b",
    kExtSync:
      "baded53eb3587d7900e604e8a68d860abf9de30b5c955d3c4d5dba63f26fd88265cd85923f6e9dcd16aef3b82bc88039a89c59ecd9e88de09a7418c7d94f90c9",
    kExtKbHash:
      "b776a89db29f22daedd154b44ff88397d0b210223fb956f5a749521dd8de8ddf",
  };
  await fxa.setSignedInUser(user);
  await Assert.rejects(
    fxa.keys.getKeyForScope(SCOPE_OLD_SYNC),
    /The FxA server did not grant Firefox the `oldsync` scope/
  );
});

// _fetchAndUnwrapAndDeriveKeys with no keyFetchToken should trigger signOut
// XXX - actually, it probably shouldn't - bug 1572313.
add_test(function test_fetchAndUnwrapAndDeriveKeys_no_token() {
  let fxa = new MockFxAccounts();
  let user = getTestUser("lettuce.protheroe");
  delete user.keyFetchToken;

  makeObserver(ONLOGOUT_NOTIFICATION, function() {
    log.debug("test_fetchAndUnwrapKeys_no_token observed logout");
    fxa._internal.getUserAccountData().then(user2 => {
      fxa._internal.abortExistingFlow().then(run_next_test);
    });
  });

  fxa
    .setSignedInUser(user)
    .then(user2 => {
      return fxa.keys._fetchAndUnwrapAndDeriveKeys();
    })
    .catch(error => {
      log.info("setSignedInUser correctly rejected");
    });
});

// Alice (User A) signs up but never verifies her email.  Then Bob (User B)
// signs in with a verified email.  Ensure that no sign-in events are triggered
// on Alice's behalf.  In the end, Bob should be the signed-in user.
add_test(function test_overlapping_signins() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  let bob = getTestUser("bob");

  makeObserver(ONVERIFIED_NOTIFICATION, function() {
    log.debug("test_overlapping_signins observed onverified");
    // Once email verification is complete, we will observe onverified
    fxa._internal.getUserAccountData().then(user => {
      Assert.equal(user.email, bob.email);
      Assert.equal(user.verified, true);
      run_next_test();
    });
  });

  // Alice is the user signing in; her email is unverified.
  fxa.setSignedInUser(alice).then(() => {
    log.debug("Alice signing in ...");
    fxa._internal.getUserAccountData().then(user => {
      Assert.equal(user.email, alice.email);
      Assert.equal(user.verified, false);
      log.debug("Alice has not verified her email ...");

      // Now Bob signs in instead and actually verifies his email
      log.debug("Bob signing in ...");
      fxa.setSignedInUser(bob).then(() => {
        do_timeout(200, function() {
          // Mock email verification ...
          log.debug("Bob verifying his email ...");
          fxa._internal.fxAccountsClient._verified = true;
        });
      });
    });
  });
});

add_task(async function test_resend_email_not_signed_in() {
  let fxa = new MockFxAccounts();

  try {
    await fxa.resendVerificationEmail();
  } catch (err) {
    Assert.equal(err.message, ERROR_NO_ACCOUNT);
    return;
  }
  do_throw("Should not be able to resend email when nobody is signed in");
});

add_task(async function test_accountStatus() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");

  // If we have no user, we have no account server-side
  let result = await fxa.checkAccountStatus();
  Assert.ok(!result);
  // Set a user - the fxAccountsClient mock will say "ok".
  await fxa.setSignedInUser(alice);
  result = await fxa.checkAccountStatus();
  Assert.ok(result);
  // flag the item as deleted on the server.
  fxa._internal.fxAccountsClient._deletedOnServer = true;
  result = await fxa.checkAccountStatus();
  Assert.ok(!result);
  fxa._internal.fxAccountsClient._deletedOnServer = false;
  await fxa.signOut();
});

add_task(async function test_resend_email_invalid_token() {
  let fxa = new MockFxAccounts();
  let sophia = getTestUser("sophia");
  Assert.notEqual(sophia.sessionToken, null);

  let client = fxa._internal.fxAccountsClient;
  client.resendVerificationEmail = () => {
    return Promise.reject({
      code: 401,
      errno: ERRNO_INVALID_AUTH_TOKEN,
    });
  };
  // This test wants the account to exist but the local session invalid.
  client.accountStatus = uid => {
    Assert.ok(uid, "got a uid to check");
    return Promise.resolve(true);
  };
  client.sessionStatus = token => {
    Assert.ok(token, "got a token to check");
    return Promise.resolve(false);
  };

  await fxa.setSignedInUser(sophia);
  let user = await fxa._internal.getUserAccountData();
  Assert.equal(user.email, sophia.email);
  Assert.equal(user.verified, false);
  log.debug("Sophia wants verification email resent");

  try {
    await fxa.resendVerificationEmail();
    Assert.ok(
      false,
      "resendVerificationEmail should reject invalid session token"
    );
  } catch (err) {
    Assert.equal(err.code, 401);
    Assert.equal(err.errno, ERRNO_INVALID_AUTH_TOKEN);
  }

  user = await fxa._internal.getUserAccountData();
  Assert.equal(user.email, sophia.email);
  Assert.equal(user.sessionToken, null);
  await fxa._internal.abortExistingFlow();
});

add_test(function test_resend_email() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");

  let initialState = fxa._internal.currentAccountState;

  // Alice is the user signing in; her email is unverified.
  fxa.setSignedInUser(alice).then(() => {
    log.debug("Alice signing in");

    // We're polling for the first email
    Assert.ok(fxa._internal.currentAccountState !== initialState);
    let aliceState = fxa._internal.currentAccountState;

    // The polling timer is ticking
    Assert.ok(fxa._internal.currentTimer > 0);

    fxa._internal.getUserAccountData().then(user => {
      Assert.equal(user.email, alice.email);
      Assert.equal(user.verified, false);
      log.debug("Alice wants verification email resent");

      fxa.resendVerificationEmail().then(result => {
        // Mock server response; ensures that the session token actually was
        // passed to the client to make the hawk call
        Assert.equal(result, "alice's session token");

        // Timer was not restarted
        Assert.ok(fxa._internal.currentAccountState === aliceState);

        // Timer is still ticking
        Assert.ok(fxa._internal.currentTimer > 0);

        // Ok abort polling before we go on to the next test
        fxa._internal.abortExistingFlow();
        run_next_test();
      });
    });
  });
});

Services.prefs.setCharPref(
  "identity.fxaccounts.remote.oauth.uri",
  "https://example.com/v1"
);

add_test(async function test_getOAuthTokenWithSessionToken() {
  Services.prefs.setBoolPref(
    "identity.fxaccounts.useSessionTokensForOAuth",
    true
  );
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;
  let oauthTokenCalled = false;

  let client = fxa._internal.fxAccountsClient;
  client.accessTokenWithSessionToken = async (
    sessionTokenHex,
    clientId,
    scope,
    ttl
  ) => {
    oauthTokenCalled = true;
    Assert.equal(sessionTokenHex, "alice's session token");
    Assert.equal(clientId, "5882386c6d801776");
    Assert.equal(scope, "profile");
    Assert.equal(ttl, undefined);
    return MOCK_TOKEN_RESPONSE;
  };

  await fxa.setSignedInUser(alice);
  const result = await fxa.getOAuthToken({ scope: "profile" });
  Assert.ok(oauthTokenCalled);
  Assert.equal(result, MOCK_TOKEN_RESPONSE.access_token);
  Services.prefs.setBoolPref(
    "identity.fxaccounts.useSessionTokensForOAuth",
    false
  );
  run_next_test();
});

add_task(async function test_getOAuthTokenCachedWithSessionToken() {
  Services.prefs.setBoolPref(
    "identity.fxaccounts.useSessionTokensForOAuth",
    true
  );
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;
  let numOauthTokenCalls = 0;

  let client = fxa._internal.fxAccountsClient;
  client.accessTokenWithSessionToken = async () => {
    numOauthTokenCalls++;
    return MOCK_TOKEN_RESPONSE;
  };

  await fxa.setSignedInUser(alice);
  let result = await fxa.getOAuthToken({
    scope: "profile",
    service: "test-service",
  });
  Assert.equal(numOauthTokenCalls, 1);
  Assert.equal(
    result,
    "43793fdfffec22eb39fc3c44ed09193a6fde4c24e5d6a73f73178597b268af69"
  );

  // requesting it again should not re-fetch the token.
  result = await fxa.getOAuthToken({
    scope: "profile",
    service: "test-service",
  });
  Assert.equal(numOauthTokenCalls, 1);
  Assert.equal(
    result,
    "43793fdfffec22eb39fc3c44ed09193a6fde4c24e5d6a73f73178597b268af69"
  );
  // But requesting the same service and a different scope *will* get a new one.
  result = await fxa.getOAuthToken({
    scope: "something-else",
    service: "test-service",
  });
  Assert.equal(numOauthTokenCalls, 2);
  Assert.equal(
    result,
    "43793fdfffec22eb39fc3c44ed09193a6fde4c24e5d6a73f73178597b268af69"
  );
  Services.prefs.setBoolPref(
    "identity.fxaccounts.useSessionTokensForOAuth",
    false
  );
});

add_test(function test_getOAuthTokenScopedWithSessionToken() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;
  let numOauthTokenCalls = 0;

  let client = fxa._internal.fxAccountsClient;
  client.accessTokenWithSessionToken = async (
    _sessionTokenHex,
    _clientId,
    scopeString
  ) => {
    equal(scopeString, "bar foo"); // scopes are sorted locally before request.
    numOauthTokenCalls++;
    return MOCK_TOKEN_RESPONSE;
  };

  fxa.setSignedInUser(alice).then(() => {
    fxa.getOAuthToken({ scope: ["foo", "bar"] }).then(result => {
      Assert.equal(numOauthTokenCalls, 1);
      Assert.equal(
        result,
        "43793fdfffec22eb39fc3c44ed09193a6fde4c24e5d6a73f73178597b268af69"
      );
      run_next_test();
    });
  });
});

add_task(async function test_getOAuthTokenCachedScopeNormalization() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;
  let numOAuthTokenCalls = 0;

  let client = fxa._internal.fxAccountsClient;
  client.accessTokenWithSessionToken = async (
    _sessionTokenHex,
    _clientId,
    scopeString
  ) => {
    numOAuthTokenCalls++;
    return MOCK_TOKEN_RESPONSE;
  };

  await fxa.setSignedInUser(alice);
  let result = await fxa.getOAuthToken({
    scope: ["foo", "bar"],
    service: "test-service",
  });
  Assert.equal(numOAuthTokenCalls, 1);
  Assert.equal(
    result,
    "43793fdfffec22eb39fc3c44ed09193a6fde4c24e5d6a73f73178597b268af69"
  );

  // requesting it again with the scope array in a different order should not re-fetch the token.
  result = await fxa.getOAuthToken({
    scope: ["bar", "foo"],
    service: "test-service",
  });
  Assert.equal(numOAuthTokenCalls, 1);
  Assert.equal(
    result,
    "43793fdfffec22eb39fc3c44ed09193a6fde4c24e5d6a73f73178597b268af69"
  );
  // requesting it again with the scope array in different case should not re-fetch the token.
  result = await fxa.getOAuthToken({
    scope: ["Bar", "Foo"],
    service: "test-service",
  });
  Assert.equal(numOAuthTokenCalls, 1);
  Assert.equal(
    result,
    "43793fdfffec22eb39fc3c44ed09193a6fde4c24e5d6a73f73178597b268af69"
  );
  // But requesting with a new entry in the array does fetch one.
  result = await fxa.getOAuthToken({
    scope: ["foo", "bar", "etc"],
    service: "test-service",
  });
  Assert.equal(numOAuthTokenCalls, 2);
  Assert.equal(
    result,
    "43793fdfffec22eb39fc3c44ed09193a6fde4c24e5d6a73f73178597b268af69"
  );
});

add_test(function test_getOAuthToken_invalid_param() {
  let fxa = new MockFxAccounts();

  fxa.getOAuthToken().catch(err => {
    Assert.equal(err.message, "INVALID_PARAMETER");
    fxa.signOut().then(run_next_test);
  });
});

add_test(function test_getOAuthToken_invalid_scope_array() {
  let fxa = new MockFxAccounts();

  fxa.getOAuthToken({ scope: [] }).catch(err => {
    Assert.equal(err.message, "INVALID_PARAMETER");
    fxa.signOut().then(run_next_test);
  });
});

add_test(function test_getOAuthToken_misconfigure_oauth_uri() {
  let fxa = new MockFxAccounts();

  const prevServerURL = Services.prefs.getCharPref(
    "identity.fxaccounts.remote.oauth.uri"
  );
  Services.prefs.deleteBranch("identity.fxaccounts.remote.oauth.uri");

  fxa.getOAuthToken().catch(err => {
    Assert.equal(err.message, "INVALID_PARAMETER");
    // revert the pref
    Services.prefs.setCharPref(
      "identity.fxaccounts.remote.oauth.uri",
      prevServerURL
    );
    fxa.signOut().then(run_next_test);
  });
});

add_test(function test_getOAuthToken_no_account() {
  let fxa = new MockFxAccounts();

  fxa._internal.currentAccountState.getUserAccountData = function() {
    return Promise.resolve(null);
  };

  fxa.getOAuthToken({ scope: "profile" }).catch(err => {
    Assert.equal(err.message, "NO_ACCOUNT");
    fxa.signOut().then(run_next_test);
  });
});

add_test(function test_getOAuthToken_unverified() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");

  fxa.setSignedInUser(alice).then(() => {
    fxa.getOAuthToken({ scope: "profile" }).catch(err => {
      Assert.equal(err.message, "UNVERIFIED_ACCOUNT");
      fxa.signOut().then(run_next_test);
    });
  });
});

add_test(function test_getOAuthToken_error() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;

  let client = fxa._internal.fxAccountsClient;
  client.accessTokenWithSessionToken = () => {
    return Promise.reject("boom");
  };

  fxa.setSignedInUser(alice).then(() => {
    fxa.getOAuthToken({ scope: "profile" }).catch(err => {
      equal(err.details, "boom");
      run_next_test();
    });
  });
});

add_task(async function test_listAttachedOAuthClients() {
  const ONE_HOUR = 60 * 60 * 1000;
  const ONE_DAY = 24 * ONE_HOUR;

  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;

  let client = fxa._internal.fxAccountsClient;
  client.attachedClients = async () => {
    return [
      // This entry was previously filtered but no longer is!
      {
        clientId: "a2270f727f45f648",
        deviceId: "deadbeef",
        sessionTokenId: null,
        name: "Firefox Preview (no session token)",
        scope: ["profile", "https://identity.mozilla.com/apps/oldsync"],
        lastAccessTime: Date.now(),
      },
      {
        clientId: "802d56ef2a9af9fa",
        deviceId: null,
        sessionTokenId: null,
        name: "Firefox Monitor",
        scope: ["profile"],
        lastAccessTime: Date.now() - ONE_DAY - ONE_HOUR,
      },
      {
        clientId: "1f30e32975ae5112",
        deviceId: null,
        sessionTokenId: null,
        name: "Firefox Send",
        scope: ["profile", "https://identity.mozilla.com/apps/send"],
        lastAccessTime: Date.now() - ONE_DAY * 2 - ONE_HOUR,
      },
      // One with a future date should be impossible, but having a negative
      // result here would almost certainly confuse something!
      {
        clientId: "future-date",
        deviceId: null,
        sessionTokenId: null,
        name: "Whatever",
        lastAccessTime: Date.now() + ONE_DAY,
      },
      // A missing/null lastAccessTime should end up with a missing lastAccessedDaysAgo
      {
        clientId: "missing-date",
        deviceId: null,
        sessionTokenId: null,
        name: "Whatever",
      },
    ];
  };

  await fxa.setSignedInUser(alice);
  const clients = await fxa.listAttachedOAuthClients();
  Assert.deepEqual(clients, [
    {
      id: "a2270f727f45f648",
      lastAccessedDaysAgo: 0,
    },
    {
      id: "802d56ef2a9af9fa",
      lastAccessedDaysAgo: 1,
    },
    {
      id: "1f30e32975ae5112",
      lastAccessedDaysAgo: 2,
    },
    {
      id: "future-date",
      lastAccessedDaysAgo: 0,
    },
    {
      id: "missing-date",
      lastAccessedDaysAgo: null,
    },
  ]);
});

add_task(async function test_getSignedInUserProfile() {
  let alice = getTestUser("alice");
  alice.verified = true;

  let mockProfile = {
    getProfile() {
      return Promise.resolve({ avatar: "image" });
    },
    tearDown() {},
  };
  let fxa = new FxAccounts({
    _signOutServer() {
      return Promise.resolve();
    },
    device: {
      _registerOrUpdateDevice() {
        return Promise.resolve();
      },
    },
  });

  await fxa._internal.setSignedInUser(alice);
  fxa._internal._profile = mockProfile;
  let result = await fxa.getSignedInUser();
  Assert.ok(!!result);
  Assert.equal(result.avatar, "image");
});

add_task(async function test_getSignedInUserProfile_error_uses_account_data() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;

  fxa._internal.getSignedInUser = function() {
    return Promise.resolve({ email: "foo@bar.com" });
  };
  fxa._internal._profile = {
    getProfile() {
      return Promise.reject("boom");
    },
    tearDown() {
      teardownCalled = true;
    },
  };

  let teardownCalled = false;
  await fxa.setSignedInUser(alice);
  let result = await fxa.getSignedInUser();
  Assert.deepEqual(result.avatar, null);
  await fxa.signOut();
  Assert.ok(teardownCalled);
});

add_task(async function test_checkVerificationStatusFailed() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;

  let client = fxa._internal.fxAccountsClient;
  client.recoveryEmailStatus = () => {
    return Promise.reject({
      code: 401,
      errno: ERRNO_INVALID_AUTH_TOKEN,
    });
  };
  client.accountStatus = () => Promise.resolve(true);
  client.sessionStatus = () => Promise.resolve(false);

  await fxa.setSignedInUser(alice);
  let user = await fxa._internal.getUserAccountData();
  Assert.notEqual(alice.sessionToken, null);
  Assert.equal(user.email, alice.email);
  Assert.equal(user.verified, true);

  await fxa._internal.checkVerificationStatus();

  user = await fxa._internal.getUserAccountData();
  Assert.equal(user.email, alice.email);
  Assert.equal(user.sessionToken, null);
});

add_task(async function test_deriveKeys() {
  let account = await MakeFxAccounts();
  let kBhex =
    "fd5c747806c07ce0b9d69dcfea144663e630b65ec4963596a22f24910d7dd15d";
  let kB = CommonUtils.hexToBytes(kBhex);
  const uid = "1ad7f502-4cc7-4ec1-a209-071fd2fae348";

  const { kSync, kXCS, kExtSync, kExtKbHash } = await account.keys._deriveKeys(
    uid,
    kB
  );

  Assert.equal(
    kSync,
    "ad501a50561be52b008878b2e0d8a73357778a712255f7722f497b5d4df14b05" +
      "dc06afb836e1521e882f521eb34691d172337accdbf6e2a5b968b05a7bbb9885"
  );
  Assert.equal(kXCS, "6ae94683571c7a7c54dab4700aa3995f");
  Assert.equal(
    kExtSync,
    "f5ccd9cfdefd9b1ac4d02c56964f59239d8dfa1ca326e63696982765c1352cdc" +
      "5d78a5a9c633a6d25edfea0a6c221a3480332a49fd866f311c2e3508ddd07395"
  );
  Assert.equal(
    kExtKbHash,
    "6192f1cc7dce95334455ba135fa1d8fca8f70e8f594ae318528de06f24ed0273"
  );
});

add_task(async function test_flushLogFile() {
  _("Tests flushLogFile");
  let account = await MakeFxAccounts();
  let promiseObserved = new Promise(res => {
    log.info("Adding flush-log-file observer.");
    Services.obs.addObserver(function onFlushLogFile() {
      Services.obs.removeObserver(
        onFlushLogFile,
        "service:log-manager:flush-log-file"
      );
      res();
    }, "service:log-manager:flush-log-file");
  });
  account.flushLogFile();
  await promiseObserved;
});

/*
 * End of tests.
 * Utility functions follow.
 */

function expandHex(two_hex) {
  // Return a 64-character hex string, encoding 32 identical bytes.
  let eight_hex = two_hex + two_hex + two_hex + two_hex;
  let thirtytwo_hex = eight_hex + eight_hex + eight_hex + eight_hex;
  return thirtytwo_hex + thirtytwo_hex;
}

function expandBytes(two_hex) {
  return CommonUtils.hexToBytes(expandHex(two_hex));
}

function getTestUser(name) {
  return {
    email: name + "@example.com",
    uid: "1ad7f5024cc74ec1a209071fd2fae348",
    sessionToken: name + "'s session token",
    keyFetchToken: name + "'s keyfetch token",
    unwrapBKey: expandHex("44"),
    verified: false,
    encryptedSendTabKeys: name + "'s encrypted Send tab keys",
  };
}

function makeObserver(aObserveTopic, aObserveFunc) {
  let observer = {
    // nsISupports provides type management in C++
    // nsIObserver is to be an observer
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

    observe(aSubject, aTopic, aData) {
      log.debug("observed " + aTopic + " " + aData);
      if (aTopic == aObserveTopic) {
        removeMe();
        aObserveFunc(aSubject, aTopic, aData);
      }
    },
  };

  function removeMe() {
    log.debug("removing observer for " + aObserveTopic);
    Services.obs.removeObserver(observer, aObserveTopic);
  }

  Services.obs.addObserver(observer, aObserveTopic);
  return removeMe;
}
