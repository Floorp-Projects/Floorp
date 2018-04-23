/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://services-common/utils.js");
ChromeUtils.import("resource://gre/modules/FxAccounts.jsm");
ChromeUtils.import("resource://gre/modules/FxAccountsClient.jsm");
ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
ChromeUtils.import("resource://gre/modules/FxAccountsOAuthGrantClient.jsm");
ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");
ChromeUtils.import("resource://gre/modules/Log.jsm");

// We grab some additional stuff via backstage passes.
var {AccountState} = ChromeUtils.import("resource://gre/modules/FxAccounts.jsm", {});

const ONE_HOUR_MS = 1000 * 60 * 60;
const ONE_DAY_MS = ONE_HOUR_MS * 24;
const TWO_MINUTES_MS = 1000 * 60 * 2;

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

function MockStorageManager() {
}

MockStorageManager.prototype = {
  promiseInitialized: Promise.resolve(),

  initialize(accountData) {
    this.accountData = accountData;
  },

  finalize() {
    return Promise.resolve();
  },

  getAccountData() {
    return Promise.resolve(this.accountData);
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
  }
};

function MockFxAccountsClient() {
  this._email = "nobody@example.com";
  this._verified = false;
  this._deletedOnServer = false; // for testing accountStatus

  // mock calls up to the auth server to determine whether the
  // user account has been verified
  this.recoveryEmailStatus = async function(sessionToken) {
    // simulate a call to /recovery_email/status
    return {
      email: this._email,
      verified: this._verified
    };
  };

  this.accountStatus = async function(uid) {
    return !!uid && (!this._deletedOnServer);
  };

  this.accountKeys = function(keyFetchToken) {
    return new Promise(resolve => {
      do_timeout(50, () => {
        resolve({
          kA: expandBytes("11"),
          wrapKB: expandBytes("22")
        });
      });
    });
  };

  this.resendVerificationEmail = function(sessionToken) {
    // Return the session token to show that we received it in the first place
    return Promise.resolve(sessionToken);
  };

  this.signCertificate = function() { throw new Error("no"); };

  this.signOut = () => Promise.resolve();

  FxAccountsClient.apply(this);
}
MockFxAccountsClient.prototype = {
  __proto__: FxAccountsClient.prototype
};

/*
 * We need to mock the FxAccounts module's interfaces to external
 * services, such as storage and the FxAccounts client.  We also
 * mock the now() method, so that we can simulate the passing of
 * time and verify that signatures expire correctly.
 */
function MockFxAccounts() {
  return new FxAccounts({
    VERIFICATION_POLL_TIMEOUT_INITIAL: 100, // 100ms

    _getCertificateSigned_calls: [],
    _d_signCertificate: PromiseUtils.defer(),
    _now_is: new Date(),
    now() {
      return this._now_is;
    },
    newAccountState(credentials) {
      // we use a real accountState but mocked storage.
      let storage = new MockStorageManager();
      storage.initialize(credentials);
      return new AccountState(storage);
    },
    getCertificateSigned(sessionToken, serializedPublicKey) {
      _("mock getCertificateSigned\n");
      this._getCertificateSigned_calls.push([sessionToken, serializedPublicKey]);
      return this._d_signCertificate.promise;
    },
    _registerOrUpdateDevice() {
      return Promise.resolve();
    },
    fxAccountsClient: new MockFxAccountsClient()
  });
}

/*
 * Some tests want a "real" fxa instance - however, we still mock the storage
 * to keep the tests fast on b2g.
 */
function MakeFxAccounts(internal = {}) {
  if (!internal.newAccountState) {
    // we use a real accountState but mocked storage.
    internal.newAccountState = function(credentials) {
      let storage = new MockStorageManager();
      storage.initialize(credentials);
      return new AccountState(storage);
    };
  }
  if (!internal._signOutServer) {
    internal._signOutServer = () => Promise.resolve();
  }
  if (!internal._registerOrUpdateDevice) {
    internal._registerOrUpdateDevice = () => Promise.resolve();
  }
  return new FxAccounts(internal);
}

add_task(async function test_get_signed_in_user_initially_unset() {
  _("Check getSignedInUser initially and after signout reports no user");
  let account = MakeFxAccounts();
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    assertion: "foobar",
    sessionToken: "dead",
    kSync: "beef",
    kXCS: "cafe",
    kExtSync: "bacon",
    kExtKbHash: "cheese",
    verified: true
  };
  let result = await account.getSignedInUser();
  Assert.equal(result, null);

  await account.setSignedInUser(credentials);
  let histogram = Services.telemetry.getHistogramById("FXA_CONFIGURED");
  Assert.equal(histogram.snapshot().sum, 1);
  histogram.clear();

  result = await account.getSignedInUser();
  Assert.equal(result.email, credentials.email);
  Assert.equal(result.assertion, credentials.assertion);
  Assert.equal(result.kSync, credentials.kSync);
  Assert.equal(result.kXCS, credentials.kXCS);
  Assert.equal(result.kExtSync, credentials.kExtSync);
  Assert.equal(result.kExtKbHash, credentials.kExtKbHash);

  // Delete the memory cache and force the user
  // to be read and parsed from storage (e.g. disk via JSONStorage).
  delete account.internal.signedInUser;
  result = await account.getSignedInUser();
  Assert.equal(result.email, credentials.email);
  Assert.equal(result.assertion, credentials.assertion);
  Assert.equal(result.kSync, credentials.kSync);
  Assert.equal(result.kXCS, credentials.kXCS);
  Assert.equal(result.kExtSync, credentials.kExtSync);
  Assert.equal(result.kExtKbHash, credentials.kExtKbHash);

  // sign out
  let localOnly = true;
  await account.signOut(localOnly);

  // user should be undefined after sign out
  result = await account.getSignedInUser();
  Assert.equal(result, null);
});

add_task(async function test_set_signed_in_user_signs_out_previous_account() {
  _("Check setSignedInUser signs out the previous account.");
  let account = MakeFxAccounts();
  let signOutServerCalled = false;
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    assertion: "foobar",
    sessionToken: "dead",
    kSync: "beef",
    kXCS: "cafe",
    kExtSync: "bacon",
    kExtKbHash: "cheese",
    verified: true
  };
  await account.setSignedInUser(credentials);

  account.internal._signOutServer = () => {
    signOutServerCalled = true;
    return Promise.resolve(true);
  };

  await account.setSignedInUser(credentials);
  Assert.ok(signOutServerCalled);
});

add_task(async function test_update_account_data() {
  _("Check updateUserAccountData does the right thing.");
  let account = MakeFxAccounts();
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    assertion: "foobar",
    sessionToken: "dead",
    kSync: "beef",
    kXCS: "cafe",
    kExtSync: "bacon",
    kExtKbHash: "cheese",
    verified: true
  };
  await account.setSignedInUser(credentials);

  let newCreds = {
    email: credentials.email,
    uid: credentials.uid,
    assertion: "new_assertion",
  };
  await account.updateUserAccountData(newCreds);
  Assert.equal((await account.getSignedInUser()).assertion, "new_assertion",
               "new field value was saved");

  // but we should fail attempting to change the uid.
  newCreds = {
    email: credentials.email,
    uid: "another_uid",
    assertion: "new_assertion",
  };
  await Assert.rejects(account.updateUserAccountData(newCreds));

  // should fail without the uid.
  newCreds = {
    assertion: "new_assertion",
  };
  await Assert.rejects(account.updateUserAccountData(newCreds));

  // and should fail with a field name that's not known by storage.
  newCreds = {
    email: credentials.email,
    uid: "another_uid",
    foo: "bar",
  };
  await Assert.rejects(account.updateUserAccountData(newCreds));
});

add_task(async function test_getCertificateOffline() {
  _("getCertificateOffline()");
  let fxa = MakeFxAccounts();
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    sessionToken: "dead",
    verified: true,
  };

  await fxa.setSignedInUser(credentials);

  // Test that an expired cert throws if we're offline.
  let offline = Services.io.offline;
  Services.io.offline = true;
  await fxa.internal.getKeypairAndCertificate(fxa.internal.currentAccountState).then(
    result => {
      Services.io.offline = offline;
      do_throw("Unexpected success");
    },
    err => {
      Services.io.offline = offline;
      // ... so we have to check the error string.
      Assert.equal(err, "Error: OFFLINE");
    }
  );
  await fxa.signOut(/* localOnly = */true);
});

add_task(async function test_getCertificateCached() {
  _("getCertificateCached()");
  let fxa = MakeFxAccounts();
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    sessionToken: "dead",
    verified: true,
    // A cached keypair and cert that remain valid.
    keyPair: {
      validUntil: Date.now() + KEY_LIFETIME + 10000,
      rawKeyPair: "good-keypair",
    },
    cert: {
      validUntil: Date.now() + CERT_LIFETIME + 10000,
      rawCert: "good-cert",
    },
  };

  await fxa.setSignedInUser(credentials);
  let {keyPair, certificate} = await fxa.internal.getKeypairAndCertificate(fxa.internal.currentAccountState);
  // should have the same keypair and cert.
  Assert.equal(keyPair, credentials.keyPair.rawKeyPair);
  Assert.equal(certificate, credentials.cert.rawCert);
  await fxa.signOut(/* localOnly = */true);
});

add_task(async function test_getCertificateExpiredCert() {
  _("getCertificateExpiredCert()");
  let fxa = MakeFxAccounts({
    getCertificateSigned() {
      return "new cert";
    }
  });
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    sessionToken: "dead",
    verified: true,
    // A cached keypair that remains valid.
    keyPair: {
      validUntil: Date.now() + KEY_LIFETIME + 10000,
      rawKeyPair: "good-keypair",
    },
    // A cached certificate which has expired.
    cert: {
      validUntil: Date.parse("Mon, 13 Jan 2000 21:45:06 GMT"),
      rawCert: "expired-cert",
    },
  };
  await fxa.setSignedInUser(credentials);
  let {keyPair, certificate} = await fxa.internal.getKeypairAndCertificate(fxa.internal.currentAccountState);
  // should have the same keypair but a new cert.
  Assert.equal(keyPair, credentials.keyPair.rawKeyPair);
  Assert.notEqual(certificate, credentials.cert.rawCert);
  await fxa.signOut(/* localOnly = */true);
});

add_task(async function test_getCertificateExpiredKeypair() {
  _("getCertificateExpiredKeypair()");
  let fxa = MakeFxAccounts({
    getCertificateSigned() {
      return "new cert";
    },
  });
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    sessionToken: "dead",
    verified: true,
    // A cached keypair that has expired.
    keyPair: {
      validUntil: Date.now() - 1000,
      rawKeyPair: "expired-keypair",
    },
    // A cached certificate which remains valid.
    cert: {
      validUntil: Date.now() + CERT_LIFETIME + 10000,
      rawCert: "expired-cert",
    },
  };

  await fxa.setSignedInUser(credentials);
  let {keyPair, certificate} = await fxa.internal.getKeypairAndCertificate(fxa.internal.currentAccountState);
  // even though the cert was valid, the fact the keypair was not means we
  // should have fetched both.
  Assert.notEqual(keyPair, credentials.keyPair.rawKeyPair);
  Assert.notEqual(certificate, credentials.cert.rawCert);
  await fxa.signOut(/* localOnly = */true);
});

// Sanity-check that our mocked client is working correctly
add_test(function test_client_mock() {
  let fxa = new MockFxAccounts();
  let client = fxa.internal.fxAccountsClient;
  Assert.equal(client._verified, false);
  Assert.equal(typeof client.signIn, "function");

  // The recoveryEmailStatus function eventually fulfills its promise
  client.recoveryEmailStatus()
    .then(response => {
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
    fxa.internal.getUserAccountData().then(user => {
      // And confirm that the user's state has changed
      Assert.equal(user.verified, true);
      Assert.equal(user.email, test_user.email);
      Assert.ok(login_notification_received);
      run_next_test();
    });
  });

  makeObserver(ONLOGIN_NOTIFICATION, function() {
    log.debug("test_verification_poll observer onlogin");
    login_notification_received = true;
  });

  fxa.setSignedInUser(test_user).then(() => {
    fxa.internal.getUserAccountData().then(user => {
      // The user is signing in, but email has not been verified yet
      Assert.equal(user.verified, false);
      do_timeout(200, function() {
        log.debug("Mocking verification of francine's email");
        fxa.internal.fxAccountsClient._email = test_user.email;
        fxa.internal.fxAccountsClient._verified = true;
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

  fxa.internal.POLL_SESSION = 1;

  let p = fxa.internal.whenVerified({});

  fxa.setSignedInUser(test_user).then(() => {
    p.then(
      (success) => {
        do_throw("this should not succeed");
      },
      (fail) => {
        removeObserver();
        fxa.signOut().then(run_next_test);
      }
    );
  });
});

add_test(function test_pollEmailStatus_start_verified() {
  let fxa = new MockFxAccounts();
  let test_user = getTestUser("carol");

  fxa.internal.POLL_SESSION = 20 * 60000;
  fxa.internal.VERIFICATION_POLL_TIMEOUT_INITIAL = 50000;

  fxa.setSignedInUser(test_user).then(() => {
    fxa.internal.getUserAccountData().then(user => {
      fxa.internal.fxAccountsClient._email = test_user.email;
      fxa.internal.fxAccountsClient._verified = true;
      const mock = sinon.mock(fxa.internal);
      mock.expects("_scheduleNextPollEmailStatus").never();
      fxa.internal.startPollEmailStatus(fxa.internal.currentAccountState, user.sessionToken, "start").then(() => {
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

  fxa.internal.POLL_SESSION = 20 * 60000;
  fxa.internal.VERIFICATION_POLL_TIMEOUT_INITIAL = 123456;

  fxa.setSignedInUser(test_user).then(() => {
    fxa.internal.getUserAccountData().then(user => {
      const mock = sinon.mock(fxa.internal);
      mock.expects("_scheduleNextPollEmailStatus").once()
          .withArgs(fxa.internal.currentAccountState, user.sessionToken, 123456, "start");
      fxa.internal.startPollEmailStatus(fxa.internal.currentAccountState, user.sessionToken, "start").then(() => {
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

  fxa.internal.POLL_SESSION = 20 * 60000;
  fxa.internal.VERIFICATION_POLL_TIMEOUT_INITIAL = 123456;
  fxa.internal.VERIFICATION_POLL_TIMEOUT_SUBSEQUENT = 654321;
  fxa.internal.VERIFICATION_POLL_START_SLOWDOWN_THRESHOLD = -1;

  fxa.setSignedInUser(test_user).then(() => {
    fxa.internal.getUserAccountData().then(user => {
      const mock = sinon.mock(fxa.internal);
      mock.expects("_scheduleNextPollEmailStatus").once()
          .withArgs(fxa.internal.currentAccountState, user.sessionToken, 654321, "start");
      fxa.internal.startPollEmailStatus(fxa.internal.currentAccountState, user.sessionToken, "start").then(() => {
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

  fxa.internal.POLL_SESSION = 20 * 60000;
  fxa.internal.VERIFICATION_POLL_TIMEOUT_SUBSEQUENT = 654321;

  fxa.setSignedInUser(test_user).then(() => {
    fxa.internal.getUserAccountData().then(user => {
      const mock = sinon.mock(fxa.internal);
      mock.expects("_scheduleNextPollEmailStatus").once()
          .withArgs(fxa.internal.currentAccountState, user.sessionToken, 654321, "browser-startup");
      fxa.internal.startPollEmailStatus(fxa.internal.currentAccountState, user.sessionToken, "browser-startup").then(() => {
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
    fxa.internal.getUserAccountData().then(user => {
      const mock = sinon.mock(fxa.internal);
      mock.expects("_scheduleNextPollEmailStatus").never();
      fxa.internal.startPollEmailStatus(fxa.internal.currentAccountState, user.sessionToken, "push").then(() => {
        mock.verify();
        mock.restore();
        run_next_test();
      });
    });
  });
});

add_test(function test_getKeys() {
  let fxa = new MockFxAccounts();
  let user = getTestUser("eusebius");

  // Once email has been verified, we will be able to get keys
  user.verified = true;

  fxa.setSignedInUser(user).then(() => {
    fxa.getSignedInUser().then((user2) => {
      // Before getKeys, we have no keys
      Assert.equal(!!user2.kSync, false);
      Assert.equal(!!user2.kXCS, false);
      Assert.equal(!!user2.kExtSync, false);
      Assert.equal(!!user2.kExtKbHash, false);
      // And we still have a key-fetch token and unwrapBKey to use
      Assert.equal(!!user2.keyFetchToken, true);
      Assert.equal(!!user2.unwrapBKey, true);

      fxa.internal.getKeys().then(() => {
        fxa.getSignedInUser().then((user3) => {
          // Now we should have keys
          Assert.equal(fxa.internal.isUserEmailVerified(user3), true);
          Assert.equal(!!user3.verified, true);
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

add_task(async function test_getKeys_kb_migration() {
  let fxa = new MockFxAccounts();
  let user = getTestUser("eusebius");

  user.verified = true;
  // Set-up the deprecated set of keys.
  user.kA = "e0245ab7f10e483470388e0a28f0a03379a3b417174fb2b42feab158b4ac2dbd";
  user.kB = "eaf9570b7219a4187d3d6bf3cec2770c2e0719b7cc0dfbb38243d6f1881675e9";

  await fxa.setSignedInUser(user);
  await fxa.internal.getKeys();
  let newUser = await fxa.getSignedInUser();
  Assert.equal(newUser.kA, null);
  Assert.equal(newUser.kB, null);
  Assert.equal(newUser.kSync, "0d6fe59791b05fa489e463ea25502e3143f6b7a903aa152e95cd9c6eddbac5b4" +
                              "dc68a19097ef65dbd147010ee45222444e66b8b3d7c8a441ebb7dd3dce015a9e");
  Assert.equal(newUser.kXCS, "22a42fe289dced5715135913424cb23b");
  Assert.equal(newUser.kExtSync, "baded53eb3587d7900e604e8a68d860abf9de30b5c955d3c4d5dba63f26fd882" +
                                   "65cd85923f6e9dcd16aef3b82bc88039a89c59ecd9e88de09a7418c7d94f90c9");
  Assert.equal(newUser.kExtKbHash, "25ed0ab3ae2f1e5365d923c9402d4255770dbe6ce79b09ed49f516985c0aa0c1");
});

add_task(async function test_getKeys_nonexistent_account() {
  let fxa = new MockFxAccounts();
  let bismarck = getTestUser("bismarck");

  let client = fxa.internal.fxAccountsClient;
  client.accountStatus = () => Promise.resolve(false);
  client.accountKeys = () => {
    return Promise.reject({
      code: 401,
      errno: ERRNO_INVALID_AUTH_TOKEN,
    });
  };

  await fxa.setSignedInUser(bismarck);

  let promiseLogout = new Promise(resolve => {
    makeObserver(ONLOGOUT_NOTIFICATION, function() {
      log.debug("test_getKeys_nonexistent_account observed logout");
      resolve();
    });
  });

  try {
    await fxa.internal.getKeys();
    Assert.ok(false);
  } catch (err) {
    Assert.equal(err.code, 401);
    Assert.equal(err.errno, ERRNO_INVALID_AUTH_TOKEN);
  }

  await promiseLogout;

  let user = await fxa.internal.getUserAccountData();
  Assert.equal(user, null);
});

// getKeys with invalid keyFetchToken should delete keyFetchToken from storage
add_task(async function test_getKeys_invalid_token() {
  let fxa = new MockFxAccounts();
  let yusuf = getTestUser("yusuf");

  let client = fxa.internal.fxAccountsClient;
  client.accountStatus = () => Promise.resolve(true);
  client.accountKeys = () => {
    return Promise.reject({
      code: 401,
      errno: ERRNO_INVALID_AUTH_TOKEN,
    });
  };

  await fxa.setSignedInUser(yusuf);

  try {
    await fxa.internal.getKeys();
    Assert.ok(false);
  } catch (err) {
    Assert.equal(err.code, 401);
    Assert.equal(err.errno, ERRNO_INVALID_AUTH_TOKEN);
  }

  let user = await fxa.internal.getUserAccountData();
  Assert.equal(user.email, yusuf.email);
  Assert.equal(user.keyFetchToken, null);
});

//  fetchAndUnwrapKeys with no keyFetchToken should trigger signOut
add_test(function test_fetchAndUnwrapKeys_no_token() {
  let fxa = new MockFxAccounts();
  let user = getTestUser("lettuce.protheroe");
  delete user.keyFetchToken;

  makeObserver(ONLOGOUT_NOTIFICATION, function() {
    log.debug("test_fetchAndUnwrapKeys_no_token observed logout");
    fxa.internal.getUserAccountData().then(user2 => {
      run_next_test();
    });
  });

  fxa.setSignedInUser(user).then(
    user2 => {
      return fxa.internal.fetchAndUnwrapKeys();
    }
  ).catch(
    error => {
      log.info("setSignedInUser correctly rejected");
    }
  );
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
    fxa.internal.getUserAccountData().then(user => {
      Assert.equal(user.email, bob.email);
      Assert.equal(user.verified, true);
      run_next_test();
    });
  });

  // Alice is the user signing in; her email is unverified.
  fxa.setSignedInUser(alice).then(() => {
    log.debug("Alice signing in ...");
    fxa.internal.getUserAccountData().then(user => {
      Assert.equal(user.email, alice.email);
      Assert.equal(user.verified, false);
      log.debug("Alice has not verified her email ...");

      // Now Bob signs in instead and actually verifies his email
      log.debug("Bob signing in ...");
      fxa.setSignedInUser(bob).then(() => {
        do_timeout(200, function() {
          // Mock email verification ...
          log.debug("Bob verifying his email ...");
          fxa.internal.fxAccountsClient._verified = true;
        });
      });
    });
  });
});

add_task(async function test_getAssertion_invalid_token() {
  let fxa = new MockFxAccounts();

  let client = fxa.internal.fxAccountsClient;
  client.accountStatus = () => Promise.resolve(true);

  let creds = {
    sessionToken: "sessionToken",
    kSync: expandHex("11"),
    kXCS: expandHex("66"),
    kExtSync: expandHex("88"),
    kExtKbHash: expandHex("22"),
    verified: true,
    email: "sonia@example.com",
  };
  await fxa.setSignedInUser(creds);
  // we have what we still believe to be a valid session token, so we should
  // consider that we have a local session.
  Assert.ok(await fxa.hasLocalSession());

  try {
    let promiseAssertion = fxa.getAssertion("audience.example.com");
    fxa.internal._d_signCertificate.reject({
      code: 401,
      errno: ERRNO_INVALID_AUTH_TOKEN,
    });
    await promiseAssertion;
    Assert.ok(false, "getAssertion should reject invalid session token");
  } catch (err) {
    Assert.equal(err.code, 401);
    Assert.equal(err.errno, ERRNO_INVALID_AUTH_TOKEN);
  }

  let user = await fxa.internal.getUserAccountData();
  Assert.equal(user.email, creds.email);
  Assert.equal(user.sessionToken, null);
  Assert.ok(!(await fxa.hasLocalSession()));
});

add_task(async function test_getAssertion() {
  let fxa = new MockFxAccounts();

  do_check_throws(async function() {
    await fxa.getAssertion("nonaudience");
  });

  let creds = {
    sessionToken: "sessionToken",
    kSync: expandHex("11"),
    kXCS: expandHex("66"),
    kExtSync: expandHex("88"),
    kExtKbHash: expandHex("22"),
    verified: true
  };
  // By putting kSync/kXCS/kExtSync/kExtKbHash/verified in "creds", we skip ahead
  // to the "we're ready" stage.
  await fxa.setSignedInUser(creds);

  _("== ready to go\n");
  // Start with a nice arbitrary but realistic date.  Here we use a nice RFC
  // 1123 date string like we would get from an HTTP header. Over the course of
  // the test, we will update 'now', but leave 'start' where it is.
  let now = Date.parse("Mon, 13 Jan 2014 21:45:06 GMT");
  let start = now;
  fxa.internal._now_is = now;

  let d = fxa.getAssertion("audience.example.com");
  // At this point, a thread has been spawned to generate the keys.
  _("-- back from fxa.getAssertion\n");
  fxa.internal._d_signCertificate.resolve("cert1");
  let assertion = await d;
  Assert.equal(fxa.internal._getCertificateSigned_calls.length, 1);
  Assert.equal(fxa.internal._getCertificateSigned_calls[0][0], "sessionToken");
  Assert.notEqual(assertion, null);
  _("ASSERTION: " + assertion + "\n");
  let pieces = assertion.split("~");
  Assert.equal(pieces[0], "cert1");
  let userData = await fxa.getSignedInUser();
  let keyPair = userData.keyPair;
  let cert = userData.cert;
  Assert.notEqual(keyPair, undefined);
  _(keyPair.validUntil + "\n");
  let p2 = pieces[1].split(".");
  let header = JSON.parse(atob(p2[0]));
  _("HEADER: " + JSON.stringify(header) + "\n");
  Assert.equal(header.alg, "DS128");
  let payload = JSON.parse(atob(p2[1]));
  _("PAYLOAD: " + JSON.stringify(payload) + "\n");
  Assert.equal(payload.aud, "audience.example.com");
  Assert.equal(keyPair.validUntil, start + KEY_LIFETIME);
  Assert.equal(cert.validUntil, start + CERT_LIFETIME);
  _("delta: " + Date.parse(payload.exp - start) + "\n");
  let exp = Number(payload.exp);

  Assert.equal(exp, now + ASSERTION_LIFETIME);

  // Reset for next call.
  fxa.internal._d_signCertificate = PromiseUtils.defer();

  // Getting a new assertion "soon" (i.e., w/o incrementing "now"), even for
  // a new audience, should not provoke key generation or a signing request.
  assertion = await fxa.getAssertion("other.example.com");

  // There were no additional calls - same number of getcert calls as before
  Assert.equal(fxa.internal._getCertificateSigned_calls.length, 1);

  // Wait an hour; assertion use period expires, but not the certificate
  now += ONE_HOUR_MS;
  fxa.internal._now_is = now;

  // This won't block on anything - will make an assertion, but not get a
  // new certificate.
  assertion = await fxa.getAssertion("third.example.com");

  // Test will time out if that failed (i.e., if that had to go get a new cert)
  pieces = assertion.split("~");
  Assert.equal(pieces[0], "cert1");
  p2 = pieces[1].split(".");
  header = JSON.parse(atob(p2[0]));
  payload = JSON.parse(atob(p2[1]));
  Assert.equal(payload.aud, "third.example.com");

  // The keypair and cert should have the same validity as before, but the
  // expiration time of the assertion should be different.  We compare this to
  // the initial start time, to which they are relative, not the current value
  // of "now".
  userData = await fxa.getSignedInUser();

  keyPair = userData.keyPair;
  cert = userData.cert;
  Assert.equal(keyPair.validUntil, start + KEY_LIFETIME);
  Assert.equal(cert.validUntil, start + CERT_LIFETIME);
  exp = Number(payload.exp);
  Assert.equal(exp, now + ASSERTION_LIFETIME);

  // Now we wait even longer, and expect both assertion and cert to expire.  So
  // we will have to get a new keypair and cert.
  now += ONE_DAY_MS;
  fxa.internal._now_is = now;
  d = fxa.getAssertion("fourth.example.com");
  fxa.internal._d_signCertificate.resolve("cert2");
  assertion = await d;
  Assert.equal(fxa.internal._getCertificateSigned_calls.length, 2);
  Assert.equal(fxa.internal._getCertificateSigned_calls[1][0], "sessionToken");
  pieces = assertion.split("~");
  Assert.equal(pieces[0], "cert2");
  p2 = pieces[1].split(".");
  header = JSON.parse(atob(p2[0]));
  payload = JSON.parse(atob(p2[1]));
  Assert.equal(payload.aud, "fourth.example.com");
  userData = await fxa.getSignedInUser();
  keyPair = userData.keyPair;
  cert = userData.cert;
  Assert.equal(keyPair.validUntil, now + KEY_LIFETIME);
  Assert.equal(cert.validUntil, now + CERT_LIFETIME);
  exp = Number(payload.exp);

  Assert.equal(exp, now + ASSERTION_LIFETIME);
  _("----- DONE ----\n");
});

add_task(async function test_resend_email_not_signed_in() {
  let fxa = new MockFxAccounts();

  try {
    await fxa.resendVerificationEmail();
  } catch (err) {
    Assert.equal(err.message,
      "Cannot resend verification email; no signed-in user");
    return;
  }
  do_throw("Should not be able to resend email when nobody is signed in");
});

add_test(function test_accountStatus() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");

  // If we have no user, we have no account server-side
  fxa.accountStatus().then(
    (result) => {
      Assert.ok(!result);
    }
  ).then(
    () => {
      fxa.setSignedInUser(alice).then(
        () => {
          fxa.accountStatus().then(
            (result) => {
               // FxAccounts.accountStatus() should match Client.accountStatus()
               Assert.ok(result);
               fxa.internal.fxAccountsClient._deletedOnServer = true;
               fxa.accountStatus().then(
                 (result2) => {
                   Assert.ok(!result2);
                   fxa.internal.fxAccountsClient._deletedOnServer = false;
                   fxa.signOut().then(run_next_test);
                 }
               );
            }
          );
        }
      );
    }
  );
});

add_task(async function test_resend_email_invalid_token() {
  let fxa = new MockFxAccounts();
  let sophia = getTestUser("sophia");
  Assert.notEqual(sophia.sessionToken, null);

  let client = fxa.internal.fxAccountsClient;
  client.resendVerificationEmail = () => {
    return Promise.reject({
      code: 401,
      errno: ERRNO_INVALID_AUTH_TOKEN,
    });
  };
  client.accountStatus = () => Promise.resolve(true);

  await fxa.setSignedInUser(sophia);
  let user = await fxa.internal.getUserAccountData();
  Assert.equal(user.email, sophia.email);
  Assert.equal(user.verified, false);
  log.debug("Sophia wants verification email resent");

  try {
    await fxa.resendVerificationEmail();
    Assert.ok(false, "resendVerificationEmail should reject invalid session token");
  } catch (err) {
    Assert.equal(err.code, 401);
    Assert.equal(err.errno, ERRNO_INVALID_AUTH_TOKEN);
  }

  user = await fxa.internal.getUserAccountData();
  Assert.equal(user.email, sophia.email);
  Assert.equal(user.sessionToken, null);
});

add_test(function test_resend_email() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");

  let initialState = fxa.internal.currentAccountState;

  // Alice is the user signing in; her email is unverified.
  fxa.setSignedInUser(alice).then(() => {
    log.debug("Alice signing in");

    // We're polling for the first email
    Assert.ok(fxa.internal.currentAccountState !== initialState);
    let aliceState = fxa.internal.currentAccountState;

    // The polling timer is ticking
    Assert.ok(fxa.internal.currentTimer > 0);

    fxa.internal.getUserAccountData().then(user => {
      Assert.equal(user.email, alice.email);
      Assert.equal(user.verified, false);
      log.debug("Alice wants verification email resent");

      fxa.resendVerificationEmail().then((result) => {
        // Mock server response; ensures that the session token actually was
        // passed to the client to make the hawk call
        Assert.equal(result, "alice's session token");

        // Timer was not restarted
        Assert.ok(fxa.internal.currentAccountState === aliceState);

        // Timer is still ticking
        Assert.ok(fxa.internal.currentTimer > 0);

        // Ok abort polling before we go on to the next test
        fxa.internal.abortExistingFlow();
        run_next_test();
      });
    });
  });
});

add_test(function test_getOAuthToken() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;
  let getTokenFromAssertionCalled = false;

  fxa.internal._d_signCertificate.resolve("cert1");

  // create a mock oauth client
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "https://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function() {
    getTokenFromAssertionCalled = true;
    return Promise.resolve({ access_token: "token" });
  };

  fxa.setSignedInUser(alice).then(
    () => {
      fxa.getOAuthToken({ scope: "profile", client }).then(
        (result) => {
           Assert.ok(getTokenFromAssertionCalled);
           Assert.equal(result, "token");
           run_next_test();
        }
      );
    }
  );

});

add_test(function test_getOAuthTokenScoped() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;
  let getTokenFromAssertionCalled = false;

  fxa.internal._d_signCertificate.resolve("cert1");

  // create a mock oauth client
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "https://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function(assertion, scopeString) {
    equal(scopeString, "foo bar");
    getTokenFromAssertionCalled = true;
    return Promise.resolve({ access_token: "token" });
  };

  fxa.setSignedInUser(alice).then(
    () => {
      fxa.getOAuthToken({ scope: ["foo", "bar"], client }).then(
        (result) => {
           Assert.ok(getTokenFromAssertionCalled);
           Assert.equal(result, "token");
           run_next_test();
        }
      );
    }
  );

});

add_task(async function test_getOAuthTokenCached() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;
  let numTokenFromAssertionCalls = 0;

  fxa.internal._d_signCertificate.resolve("cert1");

  // create a mock oauth client
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "https://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function() {
    numTokenFromAssertionCalls += 1;
    return Promise.resolve({ access_token: "token" });
  };

  await fxa.setSignedInUser(alice);
  let result = await fxa.getOAuthToken({ scope: "profile", client, service: "test-service" });
  Assert.equal(numTokenFromAssertionCalls, 1);
  Assert.equal(result, "token");

  // requesting it again should not re-fetch the token.
  result = await fxa.getOAuthToken({ scope: "profile", client, service: "test-service" });
  Assert.equal(numTokenFromAssertionCalls, 1);
  Assert.equal(result, "token");
  // But requesting the same service and a different scope *will* get a new one.
  result = await fxa.getOAuthToken({ scope: "something-else", client, service: "test-service" });
  Assert.equal(numTokenFromAssertionCalls, 2);
  Assert.equal(result, "token");
});

add_task(async function test_getOAuthTokenCachedScopeNormalization() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;
  let numTokenFromAssertionCalls = 0;

  fxa.internal._d_signCertificate.resolve("cert1");

  // create a mock oauth client
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "https://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function() {
    numTokenFromAssertionCalls += 1;
    return Promise.resolve({ access_token: "token" });
  };

  await fxa.setSignedInUser(alice);
  let result = await fxa.getOAuthToken({ scope: ["foo", "bar"], client, service: "test-service" });
  Assert.equal(numTokenFromAssertionCalls, 1);
  Assert.equal(result, "token");

  // requesting it again with the scope array in a different order not re-fetch the token.
  result = await fxa.getOAuthToken({ scope: ["bar", "foo"], client, service: "test-service" });
  Assert.equal(numTokenFromAssertionCalls, 1);
  Assert.equal(result, "token");
  // requesting it again with the scope array in different case not re-fetch the token.
  result = await fxa.getOAuthToken({ scope: ["Bar", "Foo"], client, service: "test-service" });
  Assert.equal(numTokenFromAssertionCalls, 1);
  Assert.equal(result, "token");
  // But requesting with a new entry in the array does fetch one.
  result = await fxa.getOAuthToken({ scope: ["foo", "bar", "etc"], client, service: "test-service" });
  Assert.equal(numTokenFromAssertionCalls, 2);
  Assert.equal(result, "token");
});

Services.prefs.setCharPref("identity.fxaccounts.remote.oauth.uri", "https://example.com/v1");
add_test(function test_getOAuthToken_invalid_param() {
  let fxa = new MockFxAccounts();

  fxa.getOAuthToken()
    .catch(err => {
       Assert.equal(err.message, "INVALID_PARAMETER");
       fxa.signOut().then(run_next_test);
    });
});

add_test(function test_getOAuthToken_invalid_scope_array() {
  let fxa = new MockFxAccounts();

  fxa.getOAuthToken({scope: []})
    .catch(err => {
       Assert.equal(err.message, "INVALID_PARAMETER");
       fxa.signOut().then(run_next_test);
    });
});

add_test(function test_getOAuthToken_misconfigure_oauth_uri() {
  let fxa = new MockFxAccounts();

  Services.prefs.deleteBranch("identity.fxaccounts.remote.oauth.uri");

  fxa.getOAuthToken()
    .catch(err => {
       Assert.equal(err.message, "INVALID_PARAMETER");
       // revert the pref
       Services.prefs.setCharPref("identity.fxaccounts.remote.oauth.uri", "https://example.com/v1");
       fxa.signOut().then(run_next_test);
    });
});

add_test(function test_getOAuthToken_no_account() {
  let fxa = new MockFxAccounts();

  fxa.internal.currentAccountState.getUserAccountData = function() {
    return Promise.resolve(null);
  };

  fxa.getOAuthToken({ scope: "profile" })
    .catch(err => {
       Assert.equal(err.message, "NO_ACCOUNT");
       fxa.signOut().then(run_next_test);
    });
});

add_test(function test_getOAuthToken_unverified() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");

  fxa.setSignedInUser(alice).then(() => {
    fxa.getOAuthToken({ scope: "profile" })
      .catch(err => {
         Assert.equal(err.message, "UNVERIFIED_ACCOUNT");
         fxa.signOut().then(run_next_test);
      });
  });
});

add_test(function test_getOAuthToken_network_error() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;

  fxa.internal._d_signCertificate.resolve("cert1");

  // create a mock oauth client
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "https://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function() {
    return Promise.reject(new FxAccountsOAuthGrantClientError({
      error: ERROR_NETWORK,
      errno: ERRNO_NETWORK
    }));
  };

  fxa.setSignedInUser(alice).then(() => {
    fxa.getOAuthToken({ scope: "profile", client })
      .catch(err => {
         Assert.equal(err.message, "NETWORK_ERROR");
         Assert.equal(err.details.errno, ERRNO_NETWORK);
         run_next_test();
      });
  });
});

add_test(function test_getOAuthToken_auth_error() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;

  fxa.internal._d_signCertificate.resolve("cert1");

  // create a mock oauth client
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "https://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function() {
    return Promise.reject(new FxAccountsOAuthGrantClientError({
      error: ERROR_INVALID_FXA_ASSERTION,
      errno: ERRNO_INVALID_FXA_ASSERTION
    }));
  };

  fxa.setSignedInUser(alice).then(() => {
    fxa.getOAuthToken({ scope: "profile", client })
      .catch(err => {
         Assert.equal(err.message, "AUTH_ERROR");
         Assert.equal(err.details.errno, ERRNO_INVALID_FXA_ASSERTION);
         run_next_test();
      });
  });
});

add_test(function test_getOAuthToken_unknown_error() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;

  fxa.internal._d_signCertificate.resolve("cert1");

  // create a mock oauth client
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "https://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function() {
    return Promise.reject("boom");
  };

  fxa.setSignedInUser(alice).then(() => {
    fxa.getOAuthToken({ scope: "profile", client })
      .catch(err => {
         Assert.equal(err.message, "UNKNOWN_ERROR");
         run_next_test();
      });
  });
});

add_test(function test_getSignedInUserProfile() {
  let alice = getTestUser("alice");
  alice.verified = true;

  let mockProfile = {
    getProfile() {
      return Promise.resolve({ avatar: "image" });
    },
    tearDown() {},
  };
  let fxa = new FxAccounts({
    _signOutServer() { return Promise.resolve(); },
    _registerOrUpdateDevice() { return Promise.resolve(); }
  });

  fxa.setSignedInUser(alice).then(() => {
    fxa.internal._profile = mockProfile;
    fxa.getSignedInUserProfile()
      .then(result => {
         Assert.ok(!!result);
         Assert.equal(result.avatar, "image");
         run_next_test();
      });
  });
});

add_test(function test_getSignedInUserProfile_error_uses_account_data() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;

  fxa.internal.getSignedInUser = function() {
    return Promise.resolve({ email: "foo@bar.com" });
  };

  let teardownCalled = false;
  fxa.setSignedInUser(alice).then(() => {
    fxa.internal._profile = {
      getProfile() {
        return Promise.reject("boom");
      },
      tearDown() {
        teardownCalled = true;
      }
    };

    fxa.getSignedInUserProfile()
      .catch(error => {
        Assert.equal(error.message, "UNKNOWN_ERROR");
        fxa.signOut().then(() => {
          Assert.ok(teardownCalled);
          run_next_test();
        });
      });
  });
});

add_task(async function test_checkVerificationStatusFailed() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;

  let client = fxa.internal.fxAccountsClient;
  client.recoveryEmailStatus = () => {
    return Promise.reject({
      code: 401,
      errno: ERRNO_INVALID_AUTH_TOKEN,
    });
  };
  client.accountStatus = () => Promise.resolve(true);

  await fxa.setSignedInUser(alice);
  let user = await fxa.internal.getUserAccountData();
  Assert.notEqual(alice.sessionToken, null);
  Assert.equal(user.email, alice.email);
  Assert.equal(user.verified, true);

  await fxa.checkVerificationStatus();

  user = await fxa.internal.getUserAccountData();
  Assert.equal(user.email, alice.email);
  Assert.equal(user.sessionToken, null);
});

add_test(function test_deriveKeys() {
  let account = MakeFxAccounts();
  let kBhex = "fd5c747806c07ce0b9d69dcfea144663e630b65ec4963596a22f24910d7dd15d";
  let kB = CommonUtils.hexToBytes(kBhex);
  const uid = "1ad7f502-4cc7-4ec1-a209-071fd2fae348";

  const {kSync, kXCS, kExtSync, kExtKbHash} = account.internal._deriveKeys(uid, kB);

  Assert.equal(kSync, "ad501a50561be52b008878b2e0d8a73357778a712255f7722f497b5d4df14b05" +
                      "dc06afb836e1521e882f521eb34691d172337accdbf6e2a5b968b05a7bbb9885");
  Assert.equal(kXCS, "6ae94683571c7a7c54dab4700aa3995f");
  Assert.equal(kExtSync, "f5ccd9cfdefd9b1ac4d02c56964f59239d8dfa1ca326e63696982765c1352cdc" +
                         "5d78a5a9c633a6d25edfea0a6c221a3480332a49fd866f311c2e3508ddd07395");
  Assert.equal(kExtKbHash, "6192f1cc7dce95334455ba135fa1d8fca8f70e8f594ae318528de06f24ed0273");
  run_next_test();
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
    uid: "1ad7f502-4cc7-4ec1-a209-071fd2fae348",
    sessionToken: name + "'s session token",
    keyFetchToken: name + "'s keyfetch token",
    unwrapBKey: expandHex("44"),
    verified: false
  };
}

function makeObserver(aObserveTopic, aObserveFunc) {
  let observer = {
    // nsISupports provides type management in C++
    // nsIObserver is to be an observer
    QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

    observe(aSubject, aTopic, aData) {
      log.debug("observed " + aTopic + " " + aData);
      if (aTopic == aObserveTopic) {
        removeMe();
        aObserveFunc(aSubject, aTopic, aData);
      }
    }
  };

  function removeMe() {
    log.debug("removing observer for " + aObserveTopic);
    Services.obs.removeObserver(observer, aObserveTopic);
  }

  Services.obs.addObserver(observer, aObserveTopic);
  return removeMe;
}

function do_check_throws(func, result, stack) {
  if (!stack)
    stack = Components.stack.caller;

  try {
    func();
  } catch (ex) {
    if (ex.name == result) {
      return;
    }
    do_throw("Expected result " + result + ", caught " + ex.name, stack);
  }

  if (result) {
    do_throw("Expected result " + result + ", none thrown", stack);
  }
}
