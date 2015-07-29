/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsOAuthGrantClient.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Log.jsm");

// We grab some additional stuff via backstage passes.
let {AccountState} = Cu.import("resource://gre/modules/FxAccounts.jsm", {});

const ONE_HOUR_MS = 1000 * 60 * 60;
const ONE_DAY_MS = ONE_HOUR_MS * 24;
const TWO_MINUTES_MS = 1000 * 60 * 2;

initTestLogging("Trace");

// XXX until bug 937114 is fixed
Cu.importGlobalProperties(['atob']);

let log = Log.repository.getLogger("Services.FxAccounts.test");
log.level = Log.Level.Debug;

// See verbose logging from FxAccounts.jsm
Services.prefs.setCharPref("identity.fxaccounts.loglevel", "Trace");
Log.repository.getLogger("FirefoxAccounts").level = Log.Level.Trace;

// The oauth server is mocked, but set these prefs to pass param checks
Services.prefs.setCharPref("identity.fxaccounts.remote.oauth.uri", "https://example.com/v1");
Services.prefs.setCharPref("identity.fxaccounts.oauth.client_id", "abc123");


const PROFILE_SERVER_URL = "http://example.com/v1";
const CONTENT_URL = "http://accounts.example.com/";

Services.prefs.setCharPref("identity.fxaccounts.remote.profile.uri", PROFILE_SERVER_URL);
Services.prefs.setCharPref("identity.fxaccounts.settings.uri", CONTENT_URL);

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
    for (let [name, value] of Iterator(updatedFields)) {
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
}

function MockFxAccountsClient() {
  this._email = "nobody@example.com";
  this._verified = false;
  this._deletedOnServer = false; // for testing accountStatus

  // mock calls up to the auth server to determine whether the
  // user account has been verified
  this.recoveryEmailStatus = function (sessionToken) {
    // simulate a call to /recovery_email/status
    return Promise.resolve({
      email: this._email,
      verified: this._verified
    });
  };

  this.accountStatus = function(uid) {
    let deferred = Promise.defer();
    deferred.resolve(!!uid && (!this._deletedOnServer));
    return deferred.promise;
  };

  this.accountKeys = function (keyFetchToken) {
    let deferred = Promise.defer();

    do_timeout(50, () => {
      let response = {
        kA: expandBytes("11"),
        wrapKB: expandBytes("22")
      };
      deferred.resolve(response);
    });
    return deferred.promise;
  };

  this.resendVerificationEmail = function(sessionToken) {
    // Return the session token to show that we received it in the first place
    return Promise.resolve(sessionToken);
  };

  this.signCertificate = function() { throw "no" };

  this.signOut = function() { return Promise.resolve(); };

  FxAccountsClient.apply(this);
}
MockFxAccountsClient.prototype = {
  __proto__: FxAccountsClient.prototype
}

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
    _d_signCertificate: Promise.defer(),
    _now_is: new Date(),
    now: function () {
      return this._now_is;
    },
    newAccountState(credentials) {
      // we use a real accountState but mocked storage.
      let storage = new MockStorageManager();
      storage.initialize(credentials);
      return new AccountState(storage);
    },
    getCertificateSigned: function (sessionToken, serializedPublicKey) {
      _("mock getCertificateSigned\n");
      this._getCertificateSigned_calls.push([sessionToken, serializedPublicKey]);
      return this._d_signCertificate.promise;
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
  return new FxAccounts(internal);
}

add_test(function test_non_https_remote_server_uri_with_requireHttps_false() {
  Services.prefs.setBoolPref(
    "identity.fxaccounts.allowHttp",
    true);
  Services.prefs.setCharPref(
    "identity.fxaccounts.remote.signup.uri",
    "http://example.com/browser/browser/base/content/test/general/accounts_testRemoteCommands.html");
  do_check_eq(fxAccounts.getAccountsSignUpURI(),
              "http://example.com/browser/browser/base/content/test/general/accounts_testRemoteCommands.html");

  Services.prefs.clearUserPref("identity.fxaccounts.remote.signup.uri");
  Services.prefs.clearUserPref("identity.fxaccounts.allowHttp");
  run_next_test();
});

add_test(function test_non_https_remote_server_uri() {
  Services.prefs.setCharPref(
    "identity.fxaccounts.remote.signup.uri",
    "http://example.com/browser/browser/base/content/test/general/accounts_testRemoteCommands.html");
  do_check_throws_message(function () {
    fxAccounts.getAccountsSignUpURI();
  }, "Firefox Accounts server must use HTTPS");

  Services.prefs.clearUserPref("identity.fxaccounts.remote.signup.uri");

  run_next_test();
});

add_task(function test_get_signed_in_user_initially_unset() {
  _("Check getSignedInUser initially and after signout reports no user");
  let account = MakeFxAccounts();
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    assertion: "foobar",
    sessionToken: "dead",
    kA: "beef",
    kB: "cafe",
    verified: true
  };
  let result = yield account.getSignedInUser();
  do_check_eq(result, null);

  yield account.setSignedInUser(credentials);

  result = yield account.getSignedInUser();
  do_check_eq(result.email, credentials.email);
  do_check_eq(result.assertion, credentials.assertion);
  do_check_eq(result.kB, credentials.kB);

  // Delete the memory cache and force the user
  // to be read and parsed from storage (e.g. disk via JSONStorage).
  delete account.internal.signedInUser;
  result = yield account.getSignedInUser();
  do_check_eq(result.email, credentials.email);
  do_check_eq(result.assertion, credentials.assertion);
  do_check_eq(result.kB, credentials.kB);

  // sign out
  let localOnly = true;
  yield account.signOut(localOnly);

  // user should be undefined after sign out
  result = yield account.getSignedInUser();
  do_check_eq(result, null);
});

add_task(function* test_getCertificateOffline() {
  _("getCertificateOffline()");
  let fxa = MakeFxAccounts();
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    sessionToken: "dead",
    verified: true,
  };

  yield fxa.setSignedInUser(credentials);

  // Test that an expired cert throws if we're offline.
  let offline = Services.io.offline;
  Services.io.offline = true;
  yield fxa.internal.getKeypairAndCertificate(fxa.internal.currentAccountState).then(
    result => {
      Services.io.offline = offline;
      do_throw("Unexpected success");
    },
    err => {
      Services.io.offline = offline;
      // ... so we have to check the error string.
      do_check_eq(err, "Error: OFFLINE");
    }
  );
  yield fxa.signOut(/*localOnly = */true);
});

add_task(function* test_getCertificateCached() {
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

  yield fxa.setSignedInUser(credentials);
  let {keyPair, certificate} = yield fxa.internal.getKeypairAndCertificate(fxa.internal.currentAccountState);
  // should have the same keypair and cert.
  do_check_eq(keyPair, credentials.keyPair.rawKeyPair);
  do_check_eq(certificate, credentials.cert.rawCert);
  yield fxa.signOut(/*localOnly = */true);
});

add_task(function* test_getCertificateExpiredCert() {
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
  yield fxa.setSignedInUser(credentials);
  let {keyPair, certificate} = yield fxa.internal.getKeypairAndCertificate(fxa.internal.currentAccountState);
  // should have the same keypair but a new cert.
  do_check_eq(keyPair, credentials.keyPair.rawKeyPair);
  do_check_neq(certificate, credentials.cert.rawCert);
  yield fxa.signOut(/*localOnly = */true);
});

add_task(function* test_getCertificateExpiredKeypair() {
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

  yield fxa.setSignedInUser(credentials);
  let {keyPair, certificate} = yield fxa.internal.getKeypairAndCertificate(fxa.internal.currentAccountState);
  // even though the cert was valid, the fact the keypair was not means we
  // should have fetched both.
  do_check_neq(keyPair, credentials.keyPair.rawKeyPair);
  do_check_neq(certificate, credentials.cert.rawCert);
  yield fxa.signOut(/*localOnly = */true);
});

// Sanity-check that our mocked client is working correctly
add_test(function test_client_mock() {
  let fxa = new MockFxAccounts();
  let client = fxa.internal.fxAccountsClient;
  do_check_eq(client._verified, false);
  do_check_eq(typeof client.signIn, "function");

  // The recoveryEmailStatus function eventually fulfills its promise
  client.recoveryEmailStatus()
    .then(response => {
      do_check_eq(response.verified, false);
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
      do_check_eq(user.verified, true);
      do_check_eq(user.email, test_user.email);
      do_check_true(login_notification_received);
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
      do_check_eq(user.verified, false);
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

add_test(function test_getKeys() {
  let fxa = new MockFxAccounts();
  let user = getTestUser("eusebius");

  // Once email has been verified, we will be able to get keys
  user.verified = true;

  fxa.setSignedInUser(user).then(() => {
    fxa.getSignedInUser().then((user) => {
      // Before getKeys, we have no keys
      do_check_eq(!!user.kA, false);
      do_check_eq(!!user.kB, false);
      // And we still have a key-fetch token and unwrapBKey to use
      do_check_eq(!!user.keyFetchToken, true);
      do_check_eq(!!user.unwrapBKey, true);

      fxa.internal.getKeys().then(() => {
        fxa.getSignedInUser().then((user) => {
          // Now we should have keys
          do_check_eq(fxa.internal.isUserEmailVerified(user), true);
          do_check_eq(!!user.verified, true);
          do_check_eq(user.kA, expandHex("11"));
          do_check_eq(user.kB, expandHex("66"));
          do_check_eq(user.keyFetchToken, undefined);
          do_check_eq(user.unwrapBKey, undefined);
          run_next_test();
        });
      });
    });
  });
});

//  fetchAndUnwrapKeys with no keyFetchToken should trigger signOut
add_test(function test_fetchAndUnwrapKeys_no_token() {
  let fxa = new MockFxAccounts();
  let user = getTestUser("lettuce.protheroe");
  delete user.keyFetchToken

  makeObserver(ONLOGOUT_NOTIFICATION, function() {
    log.debug("test_fetchAndUnwrapKeys_no_token observed logout");
    fxa.internal.getUserAccountData().then(user => {
      run_next_test();
    });
  });

  fxa.setSignedInUser(user).then(
    user => {
      return fxa.internal.fetchAndUnwrapKeys();
    }
  ).then(
    null,
    error => {
      log.info("setSignedInUser correctly rejected");
    }
  )
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
      do_check_eq(user.email, bob.email);
      do_check_eq(user.verified, true);
      run_next_test();
    });
  });

  // Alice is the user signing in; her email is unverified.
  fxa.setSignedInUser(alice).then(() => {
    log.debug("Alice signing in ...");
    fxa.internal.getUserAccountData().then(user => {
      do_check_eq(user.email, alice.email);
      do_check_eq(user.verified, false);
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

add_task(function test_getAssertion() {
  let fxa = new MockFxAccounts();

  do_check_throws(function() {
    yield fxa.getAssertion("nonaudience");
  });

  let creds = {
    sessionToken: "sessionToken",
    kA: expandHex("11"),
    kB: expandHex("66"),
    verified: true
  };
  // By putting kA/kB/verified in "creds", we skip ahead
  // to the "we're ready" stage.
  yield fxa.setSignedInUser(creds);

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
  let assertion = yield d;
  do_check_eq(fxa.internal._getCertificateSigned_calls.length, 1);
  do_check_eq(fxa.internal._getCertificateSigned_calls[0][0], "sessionToken");
  do_check_neq(assertion, null);
  _("ASSERTION: " + assertion + "\n");
  let pieces = assertion.split("~");
  do_check_eq(pieces[0], "cert1");
  let userData = yield fxa.getSignedInUser();
  let keyPair = userData.keyPair;
  let cert = userData.cert;
  do_check_neq(keyPair, undefined);
  _(keyPair.validUntil + "\n");
  let p2 = pieces[1].split(".");
  let header = JSON.parse(atob(p2[0]));
  _("HEADER: " + JSON.stringify(header) + "\n");
  do_check_eq(header.alg, "DS128");
  let payload = JSON.parse(atob(p2[1]));
  _("PAYLOAD: " + JSON.stringify(payload) + "\n");
  do_check_eq(payload.aud, "audience.example.com");
  do_check_eq(keyPair.validUntil, start + KEY_LIFETIME);
  do_check_eq(cert.validUntil, start + CERT_LIFETIME);
  _("delta: " + Date.parse(payload.exp - start) + "\n");
  let exp = Number(payload.exp);

  do_check_eq(exp, now + ASSERTION_LIFETIME);

  // Reset for next call.
  fxa.internal._d_signCertificate = Promise.defer();

  // Getting a new assertion "soon" (i.e., w/o incrementing "now"), even for
  // a new audience, should not provoke key generation or a signing request.
  assertion = yield fxa.getAssertion("other.example.com");

  // There were no additional calls - same number of getcert calls as before
  do_check_eq(fxa.internal._getCertificateSigned_calls.length, 1);

  // Wait an hour; assertion use period expires, but not the certificate
  now += ONE_HOUR_MS;
  fxa.internal._now_is = now;

  // This won't block on anything - will make an assertion, but not get a
  // new certificate.
  assertion = yield fxa.getAssertion("third.example.com");

  // Test will time out if that failed (i.e., if that had to go get a new cert)
  pieces = assertion.split("~");
  do_check_eq(pieces[0], "cert1");
  p2 = pieces[1].split(".");
  header = JSON.parse(atob(p2[0]));
  payload = JSON.parse(atob(p2[1]));
  do_check_eq(payload.aud, "third.example.com");

  // The keypair and cert should have the same validity as before, but the
  // expiration time of the assertion should be different.  We compare this to
  // the initial start time, to which they are relative, not the current value
  // of "now".
  userData = yield fxa.getSignedInUser();

  keyPair = userData.keyPair;
  cert = userData.cert;
  do_check_eq(keyPair.validUntil, start + KEY_LIFETIME);
  do_check_eq(cert.validUntil, start + CERT_LIFETIME);
  exp = Number(payload.exp);
  do_check_eq(exp, now + ASSERTION_LIFETIME);

  // Now we wait even longer, and expect both assertion and cert to expire.  So
  // we will have to get a new keypair and cert.
  now += ONE_DAY_MS;
  fxa.internal._now_is = now;
  d = fxa.getAssertion("fourth.example.com");
  fxa.internal._d_signCertificate.resolve("cert2");
  assertion = yield d;
  do_check_eq(fxa.internal._getCertificateSigned_calls.length, 2);
  do_check_eq(fxa.internal._getCertificateSigned_calls[1][0], "sessionToken");
  pieces = assertion.split("~");
  do_check_eq(pieces[0], "cert2");
  p2 = pieces[1].split(".");
  header = JSON.parse(atob(p2[0]));
  payload = JSON.parse(atob(p2[1]));
  do_check_eq(payload.aud, "fourth.example.com");
  userData = yield fxa.getSignedInUser();
  keyPair = userData.keyPair;
  cert = userData.cert;
  do_check_eq(keyPair.validUntil, now + KEY_LIFETIME);
  do_check_eq(cert.validUntil, now + CERT_LIFETIME);
  exp = Number(payload.exp);

  do_check_eq(exp, now + ASSERTION_LIFETIME);
  _("----- DONE ----\n");
});

add_task(function test_resend_email_not_signed_in() {
  let fxa = new MockFxAccounts();

  try {
    yield fxa.resendVerificationEmail();
  } catch(err) {
    do_check_eq(err.message,
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
      do_check_false(result);
    }
  ).then(
    () => {
      fxa.setSignedInUser(alice).then(
        () => {
          fxa.accountStatus().then(
            (result) => {
               // FxAccounts.accountStatus() should match Client.accountStatus()
               do_check_true(result);
               fxa.internal.fxAccountsClient._deletedOnServer = true;
               fxa.accountStatus().then(
                 (result) => {
                   do_check_false(result);
                   fxa.internal.fxAccountsClient._deletedOnServer = false;
                   fxa.signOut().then(run_next_test);
                 }
               );
            }
          )
        }
      );
    }
  );
});

add_test(function test_resend_email() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");

  let initialState = fxa.internal.currentAccountState;

  // Alice is the user signing in; her email is unverified.
  fxa.setSignedInUser(alice).then(() => {
    log.debug("Alice signing in");

    // We're polling for the first email
    do_check_true(fxa.internal.currentAccountState !== initialState);
    let aliceState = fxa.internal.currentAccountState;

    // The polling timer is ticking
    do_check_true(fxa.internal.currentTimer > 0);

    fxa.internal.getUserAccountData().then(user => {
      do_check_eq(user.email, alice.email);
      do_check_eq(user.verified, false);
      log.debug("Alice wants verification email resent");

      fxa.resendVerificationEmail().then((result) => {
        // Mock server response; ensures that the session token actually was
        // passed to the client to make the hawk call
        do_check_eq(result, "alice's session token");

        // Timer was not restarted
        do_check_true(fxa.internal.currentAccountState === aliceState);

        // Timer is still ticking
        do_check_true(fxa.internal.currentTimer > 0);

        // Ok abort polling before we go on to the next test
        fxa.internal.abortExistingFlow();
        run_next_test();
      });
    });
  });
});

add_test(function test_sign_out() {
  let fxa = new MockFxAccounts();
  let remoteSignOutCalled = false;
  let client = fxa.internal.fxAccountsClient;
  client.signOut = function() { remoteSignOutCalled = true; return Promise.resolve(); };
  makeObserver(ONLOGOUT_NOTIFICATION, function() {
    log.debug("test_sign_out_with_remote_error observed onlogout");
    // user should be undefined after sign out
    fxa.internal.getUserAccountData().then(user => {
      do_check_eq(user, null);
      do_check_true(remoteSignOutCalled);
      run_next_test();
    });
  });
  fxa.signOut();
});

add_test(function test_sign_out_with_remote_error() {
  let fxa = new MockFxAccounts();
  let client = fxa.internal.fxAccountsClient;
  let remoteSignOutCalled = false;
  // Force remote sign out to trigger an error
  client.signOut = function() { remoteSignOutCalled = true; throw "Remote sign out error"; };
  makeObserver(ONLOGOUT_NOTIFICATION, function() {
    log.debug("test_sign_out_with_remote_error observed onlogout");
    // user should be undefined after sign out
    fxa.internal.getUserAccountData().then(user => {
      do_check_eq(user, null);
      do_check_true(remoteSignOutCalled);
      run_next_test();
    });
  });
  fxa.signOut();
});

add_test(function test_getOAuthToken() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;
  let getTokenFromAssertionCalled = false;

  fxa.internal._d_signCertificate.resolve("cert1");

  // create a mock oauth client
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "http://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function () {
    getTokenFromAssertionCalled = true;
    return Promise.resolve({ access_token: "token" });
  };

  fxa.setSignedInUser(alice).then(
    () => {
      fxa.getOAuthToken({ scope: "profile", client: client }).then(
        (result) => {
           do_check_true(getTokenFromAssertionCalled);
           do_check_eq(result, "token");
           run_next_test();
        }
      )
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
    serverURL: "http://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function (assertion, scopeString) {
    equal(scopeString, "foo bar");
    getTokenFromAssertionCalled = true;
    return Promise.resolve({ access_token: "token" });
  };

  fxa.setSignedInUser(alice).then(
    () => {
      fxa.getOAuthToken({ scope: ["foo", "bar"], client: client }).then(
        (result) => {
           do_check_true(getTokenFromAssertionCalled);
           do_check_eq(result, "token");
           run_next_test();
        }
      )
    }
  );

});

add_task(function* test_getOAuthTokenCached() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;
  let numTokenFromAssertionCalls = 0;

  fxa.internal._d_signCertificate.resolve("cert1");

  // create a mock oauth client
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "http://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function () {
    numTokenFromAssertionCalls += 1;
    return Promise.resolve({ access_token: "token" });
  };

  yield fxa.setSignedInUser(alice);
  let result = yield fxa.getOAuthToken({ scope: "profile", client: client, service: "test-service" });
  do_check_eq(numTokenFromAssertionCalls, 1);
  do_check_eq(result, "token");

  // requesting it again should not re-fetch the token.
  result = yield fxa.getOAuthToken({ scope: "profile", client: client, service: "test-service" });
  do_check_eq(numTokenFromAssertionCalls, 1);
  do_check_eq(result, "token");
  // But requesting the same service and a different scope *will* get a new one.
  result = yield fxa.getOAuthToken({ scope: "something-else", client: client, service: "test-service" });
  do_check_eq(numTokenFromAssertionCalls, 2);
  do_check_eq(result, "token");
});

add_task(function* test_getOAuthTokenCachedScopeNormalization() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;
  let numTokenFromAssertionCalls = 0;

  fxa.internal._d_signCertificate.resolve("cert1");

  // create a mock oauth client
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "http://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function () {
    numTokenFromAssertionCalls += 1;
    return Promise.resolve({ access_token: "token" });
  };

  yield fxa.setSignedInUser(alice);
  let result = yield fxa.getOAuthToken({ scope: ["foo", "bar"], client: client, service: "test-service" });
  do_check_eq(numTokenFromAssertionCalls, 1);
  do_check_eq(result, "token");

  // requesting it again with the scope array in a different order not re-fetch the token.
  result = yield fxa.getOAuthToken({ scope: ["bar", "foo"], client: client, service: "test-service" });
  do_check_eq(numTokenFromAssertionCalls, 1);
  do_check_eq(result, "token");
  // requesting it again with the scope array in different case not re-fetch the token.
  result = yield fxa.getOAuthToken({ scope: ["Bar", "Foo"], client: client, service: "test-service" });
  do_check_eq(numTokenFromAssertionCalls, 1);
  do_check_eq(result, "token");
  // But requesting with a new entry in the array does fetch one.
  result = yield fxa.getOAuthToken({ scope: ["foo", "bar", "etc"], client: client, service: "test-service" });
  do_check_eq(numTokenFromAssertionCalls, 2);
  do_check_eq(result, "token");
});

Services.prefs.setCharPref("identity.fxaccounts.remote.oauth.uri", "https://example.com/v1");
add_test(function test_getOAuthToken_invalid_param() {
  let fxa = new MockFxAccounts();

  fxa.getOAuthToken()
    .then(null, err => {
       do_check_eq(err.message, "INVALID_PARAMETER");
       fxa.signOut().then(run_next_test);
    });
});

add_test(function test_getOAuthToken_invalid_scope_array() {
  let fxa = new MockFxAccounts();

  fxa.getOAuthToken({scope: []})
    .then(null, err => {
       do_check_eq(err.message, "INVALID_PARAMETER");
       fxa.signOut().then(run_next_test);
    });
});

add_test(function test_getOAuthToken_misconfigure_oauth_uri() {
  let fxa = new MockFxAccounts();

  Services.prefs.deleteBranch("identity.fxaccounts.remote.oauth.uri");

  fxa.getOAuthToken()
    .then(null, err => {
       do_check_eq(err.message, "INVALID_PARAMETER");
       // revert the pref
       Services.prefs.setCharPref("identity.fxaccounts.remote.oauth.uri", "https://example.com/v1");
       fxa.signOut().then(run_next_test);
    });
});

add_test(function test_getOAuthToken_no_account() {
  let fxa = new MockFxAccounts();

  fxa.internal.currentAccountState.getUserAccountData = function () {
    return Promise.resolve(null);
  };

  fxa.getOAuthToken({ scope: "profile" })
    .then(null, err => {
       do_check_eq(err.message, "NO_ACCOUNT");
       fxa.signOut().then(run_next_test);
    });
});

add_test(function test_getOAuthToken_unverified() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");

  fxa.setSignedInUser(alice).then(() => {
    fxa.getOAuthToken({ scope: "profile" })
      .then(null, err => {
         do_check_eq(err.message, "UNVERIFIED_ACCOUNT");
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
    serverURL: "http://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function () {
    return Promise.reject(new FxAccountsOAuthGrantClientError({
      error: ERROR_NETWORK,
      errno: ERRNO_NETWORK
    }));
  };

  fxa.setSignedInUser(alice).then(() => {
    fxa.getOAuthToken({ scope: "profile", client: client })
      .then(null, err => {
         do_check_eq(err.message, "NETWORK_ERROR");
         do_check_eq(err.details.errno, ERRNO_NETWORK);
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
    serverURL: "http://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function () {
    return Promise.reject(new FxAccountsOAuthGrantClientError({
      error: ERROR_INVALID_FXA_ASSERTION,
      errno: ERRNO_INVALID_FXA_ASSERTION
    }));
  };

  fxa.setSignedInUser(alice).then(() => {
    fxa.getOAuthToken({ scope: "profile", client: client })
      .then(null, err => {
         do_check_eq(err.message, "AUTH_ERROR");
         do_check_eq(err.details.errno, ERRNO_INVALID_FXA_ASSERTION);
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
    serverURL: "http://example.com/v1",
    client_id: "abc123"
  });
  client.getTokenFromAssertion = function () {
    return Promise.reject("boom");
  };

  fxa.setSignedInUser(alice).then(() => {
    fxa.getOAuthToken({ scope: "profile", client: client })
      .then(null, err => {
         do_check_eq(err.message, "UNKNOWN_ERROR");
         run_next_test();
      });
  });
});

add_test(function test_getSignedInUserProfile() {
  let alice = getTestUser("alice");
  alice.verified = true;

  let mockProfile = {
    getProfile: function () {
      return Promise.resolve({ avatar: "image" });
    },
    tearDown: function() {},
  };
  let fxa = new FxAccounts({});

  fxa.setSignedInUser(alice).then(() => {
    fxa.internal._profile = mockProfile;
    fxa.getSignedInUserProfile()
      .then(result => {
         do_check_true(!!result);
         do_check_eq(result.avatar, "image");
         run_next_test();
      });
  });
});

add_test(function test_getSignedInUserProfile_error_uses_account_data() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  alice.verified = true;

  fxa.internal.getSignedInUser = function () {
    return Promise.resolve({ email: "foo@bar.com" });
  };

  let teardownCalled = false;
  fxa.setSignedInUser(alice).then(() => {
    fxa.internal._profile = {
      getProfile: function () {
        return Promise.reject("boom");
      },
      tearDown: function() {
        teardownCalled = true;
      }
    };

    fxa.getSignedInUserProfile()
      .catch(error => {
        do_check_eq(error.message, "UNKNOWN_ERROR");
        fxa.signOut().then(() => {
          do_check_true(teardownCalled);
          run_next_test();
        });
      });
  });
});

add_test(function test_getSignedInUserProfile_unverified_account() {
  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");

  fxa.setSignedInUser(alice).then(() => {
    fxa.getSignedInUserProfile()
      .catch(error => {
         do_check_eq(error.message, "UNVERIFIED_ACCOUNT");
         fxa.signOut().then(run_next_test);
      });
  });

});

add_test(function test_getSignedInUserProfile_no_account_data() {
  let fxa = new MockFxAccounts();

  fxa.internal.getSignedInUser = function () {
    return Promise.resolve(null);
  };

  fxa.getSignedInUserProfile()
    .catch(error => {
       do_check_eq(error.message, "NO_ACCOUNT");
       fxa.signOut().then(run_next_test);
    });

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
};

function expandBytes(two_hex) {
  return CommonUtils.hexToBytes(expandHex(two_hex));
};

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
    QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

    observe: function (aSubject, aTopic, aData) {
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

  Services.obs.addObserver(observer, aObserveTopic, false);
  return removeMe;
}

function do_check_throws(func, result, stack)
{
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
