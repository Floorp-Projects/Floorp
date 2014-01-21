/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Log.jsm");

// XXX until bug 937114 is fixed
Cu.importGlobalProperties(['atob']);

let log = Log.repository.getLogger("Services.FxAccounts.test");
log.level = Log.Level.Debug;

// See verbose logging from FxAccounts.jsm
Services.prefs.setCharPref("identity.fxaccounts.loglevel", "DEBUG");

function run_test() {
  run_next_test();
}

/*
 * The FxAccountsClient communicates with the remote Firefox
 * Accounts auth server.  Mock the server calls, with a little
 * lag time to simulate some latency.
 *
 * We add the _verified attribute to mock the change in verification
 * state on the FXA server.
 */
function MockFxAccountsClient() {
  this._email = "nobody@example.com";
  this._verified = false;

  // mock calls up to the auth server to determine whether the
  // user account has been verified
  this.recoveryEmailStatus = function (sessionToken) {
    // simulate a call to /recovery_email/status
    let deferred = Promise.defer();

    let response = {
      email: this._email,
      verified: this._verified
    };
    deferred.resolve(response);

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

  this.signCertificate = function() { throw "no" };

  FxAccountsClient.apply(this);
}
MockFxAccountsClient.prototype = {
  __proto__: FxAccountsClient.prototype
}

let MockStorage = function() {
  this.data = null;
};
MockStorage.prototype = Object.freeze({
  set: function (contents) {
    this.data = contents;
    return Promise.resolve(null);
  },
  get: function () {
    return Promise.resolve(this.data);
  },
});

/*
 * We need to mock the FxAccounts module's interfaces to external
 * services, such as storage and the FxAccounts client.  We also
 * mock the now() method, so that we can simulate the passing of
 * time and verify that signatures expire correctly.
 */
let MockFxAccounts = function() {
  this._getCertificateSigned_calls = [];
  this._d_signCertificate = Promise.defer();
  this._now_is = new Date();

  let mockInternal = {
    signedInUserStorage: new MockStorage(),
    now: () => {
      return this._now_is;
    },
    getCertificateSigned: (sessionToken, serializedPublicKey) => {
      _("mock getCerificateSigned\n");
      this._getCertificateSigned_calls.push([sessionToken, serializedPublicKey]);
      return this._d_signCertificate.promise;
    },
    fxAccountsClient: new MockFxAccountsClient()
  };
  FxAccounts.apply(this, [mockInternal]);
};
MockFxAccounts.prototype = {
  __proto__: FxAccounts.prototype,
};

add_test(function test_non_https_remote_server_uri() {
  Services.prefs.setCharPref(
    "identity.fxaccounts.remote.uri",
    "http://example.com/browser/browser/base/content/test/general/accounts_testRemoteCommands.html");
  do_check_throws_message(function () {
    fxAccounts.getAccountsURI();
  }, "Firefox Accounts server must use HTTPS");

  Services.prefs.clearUserPref("identity.fxaccounts.remote.uri");

  run_next_test();
});

add_task(function test_get_signed_in_user_initially_unset() {
  // This test, unlike the rest, uses an un-mocked FxAccounts instance.
  // However, we still need to pass an object to the constructor to
  // force it to expose "internal", so we can test the disk storage.
  let account = new FxAccounts({onlySetInternal: true})
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

  let result = yield account.getSignedInUser();
  do_check_eq(result.email, credentials.email);
  do_check_eq(result.assertion, credentials.assertion);
  do_check_eq(result.kB, credentials.kB);

  // Delete the memory cache and force the user
  // to be read and parsed from storage (e.g. disk via JSONStorage).
  delete account.internal.signedInUser;
  let result = yield account.getSignedInUser();
  do_check_eq(result.email, credentials.email);
  do_check_eq(result.assertion, credentials.assertion);
  do_check_eq(result.kB, credentials.kB);

  // sign out
  yield account.signOut();

  // user should be undefined after sign out
  let result = yield account.getSignedInUser();
  do_check_eq(result, null);
});

/*
 * Sanity-check that our mocked client is working correctly
 */
add_test(function test_client_mock() {
  do_test_pending();

  let fxa = new MockFxAccounts();
  let client = fxa.internal.fxAccountsClient;
  do_check_eq(client._verified, false);
  do_check_eq(typeof client.signIn, "function");

  // The recoveryEmailStatus function eventually fulfills its promise
  client.recoveryEmailStatus()
    .then(response => {
      do_check_eq(response.verified, false);
      do_test_finished();
      run_next_test();
    });
});

/*
 * Sign in a user, and after a little while, verify the user's email.
 * Right after signing in the user, we should get the 'onlogin' notification.
 * Polling should detect that the email is verified, and eventually
 * 'onverified' should be observed
 */
add_test(function test_verification_poll() {
  do_test_pending();

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
      do_test_finished();
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
        // Mock email verification ...
        fxa.internal.fxAccountsClient._email = test_user.email;
        fxa.internal.fxAccountsClient._verified = true;
      });
    });
  });
});

/*
 * Sign in the user, but never verify the email.  The check-email
 * poll should time out.  No verifiedlogin event should be observed, and the
 * internal whenVerified promise should be rejected
 */
add_test(function test_polling_timeout() {
  do_test_pending();

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
  fxa.internal.POLL_STEP = 2;

  let p = fxa.internal.whenVerified({});

  fxa.setSignedInUser(test_user).then(() => {
    p.then(
      (success) => {
        do_throw("this should not succeed");
      },
      (fail) => {
        removeObserver();
        do_test_finished();
        run_next_test();
      }
    );
  });
});

add_test(function test_getKeys() {
  do_test_pending();
  let fxa = new MockFxAccounts();
  let user = getTestUser("eusebius");

  // Once email has been verified, we will be able to get keys
  user.verified = true;

  fxa.setSignedInUser(user).then(() => {
    fxa.getSignedInUser().then((user) => {
      // Before getKeys, we have no keys
      do_check_eq(!!user.kA, false);
      do_check_eq(!!user.kB, false);
      // And we still have a key-fetch token to use
      do_check_eq(!!user.keyFetchToken, true);

      fxa.internal.getKeys().then(() => {
        fxa.getSignedInUser().then((user) => {
          // Now we should have keys
          do_check_eq(fxa.internal.isUserEmailVerified(user), true);
          do_check_eq(!!user.verified, true);
          do_check_eq(user.kA, expandHex("11"));
          do_check_eq(user.kB, expandHex("66"));
          do_check_eq(user.keyFetchToken, undefined);
          do_test_finished();
          run_next_test();
        });
      });
    });
  });
});

/*
 * getKeys with no keyFetchToken should trigger signOut
 */
add_test(function test_getKeys_no_token() {
  do_test_pending();

  let fxa = new MockFxAccounts();
  let user = getTestUser("lettuce.protheroe");
  delete user.keyFetchToken

  makeObserver(ONLOGOUT_NOTIFICATION, function() {
    log.debug("test_getKeys_no_token observed logout");
    fxa.internal.getUserAccountData().then(user => {
      do_test_finished();
      run_next_test();
    });
  });

  fxa.setSignedInUser(user).then((user) => {
    fxa.internal.getKeys();
  });
});

/*
 * Alice (User A) signs up but never verifies her email.  Then Bob (User B)
 * signs in with a verified email.  Ensure that no sign-in events are triggered
 * on Alice's behalf.  In the end, Bob should be the signed-in user.
 */
add_test(function test_overlapping_signins() {
  do_test_pending();

  let fxa = new MockFxAccounts();
  let alice = getTestUser("alice");
  let bob = getTestUser("bob");

  makeObserver(ONVERIFIED_NOTIFICATION, function() {
    log.debug("test_overlapping_signins observed onverified");
    // Once email verification is complete, we will observe onverified
    fxa.internal.getUserAccountData().then(user => {
      do_check_eq(user.email, bob.email);
      do_check_eq(user.verified, true);
      do_test_finished();
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
  let now = 138000000*1000;
  let start = Date.now();
  fxa._now_is = now;
  let d = fxa.getAssertion("audience.example.com");
  // At this point, a thread has been spawned to generate the keys.
  _("-- back from fxa.getAssertion\n");
  fxa._d_signCertificate.resolve("cert1");
  let assertion = yield d;
  let finish = Date.now();
  do_check_eq(fxa._getCertificateSigned_calls.length, 1);
  do_check_eq(fxa._getCertificateSigned_calls[0][0], "sessionToken");
  do_check_neq(assertion, null);
  _("ASSERTION: "+assertion+"\n");
  let pieces = assertion.split("~");
  do_check_eq(pieces[0], "cert1");
  do_check_neq(fxa.internal.keyPair, undefined);
  _(fxa.internal.keyPair.validUntil+"\n");
  let p2 = pieces[1].split(".");
  let header = JSON.parse(atob(p2[0]));
  _("HEADER: "+JSON.stringify(header)+"\n");
  do_check_eq(header.alg, "DS128");
  let payload = JSON.parse(atob(p2[1]));
  _("PAYLOAD: "+JSON.stringify(payload)+"\n");
  do_check_eq(payload.aud, "audience.example.com");
  // FxAccounts KEY_LIFETIME
  do_check_eq(fxa.internal.keyPair.validUntil, now + (12*3600*1000));
  // FxAccounts CERT_LIFETIME
  do_check_eq(fxa.internal.cert.validUntil, now + (6*3600*1000));
  _("delta: "+(new Date(payload.exp) - now)+"\n");
  let exp = Number(payload.exp);
  // jwcrypto.jsm uses an unmocked Date.now()+2min to decide on the
  // expiration time, so we test that it's inside a specific timebox
  do_check_true(start + 2*60*1000 <= exp);
  do_check_true(exp <= finish + 2*60*1000);

  // Reset for next call.
  fxa._d_signCertificate = Promise.defer();

  // Getting a new assertion "soon" (i.e. w/o incrementing "now"), even for
  // a new audience, should not provoke key generation or a signing request.
  assertion = yield fxa.getAssertion("other.example.com");
  do_check_eq(fxa._getCertificateSigned_calls.length, 1);

  // But "waiting" (i.e. incrementing "now") will need a new key+signature.
  fxa._now_is = now + 24*3600*1000;
  start = Date.now();
  d = fxa.getAssertion("third.example.com");
  fxa._d_signCertificate.resolve("cert2");
  assertion = yield d;
  finish = Date.now();
  do_check_eq(fxa._getCertificateSigned_calls.length, 2);
  do_check_eq(fxa._getCertificateSigned_calls[1][0], "sessionToken");
  pieces = assertion.split("~");
  do_check_eq(pieces[0], "cert2");
  p2 = pieces[1].split(".");
  header = JSON.parse(atob(p2[0]));
  payload = JSON.parse(atob(p2[1]));
  do_check_eq(payload.aud, "third.example.com");
  // 12*3600*1000 === FxAccounts KEY_LIFETIME
  do_check_eq(fxa.internal.keyPair.validUntil, now + 24*3600*1000 + (12*3600*1000));
  // 6*3600*1000 === FxAccounts CERT_LIFETIME
  do_check_eq(fxa.internal.cert.validUntil, now + 24*3600*1000 + (6*3600*1000));
  exp = Number(payload.exp);
  do_check_true(start + 2*60*1000 <= exp);
  do_check_true(exp <= finish + 2*60*1000);

  _("----- DONE ----\n");
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
    do_throw("Expected result " + result + ", caught " + ex, stack);
  }

  if (result) {
    do_throw("Expected result " + result + ", none thrown", stack);
  }
}
