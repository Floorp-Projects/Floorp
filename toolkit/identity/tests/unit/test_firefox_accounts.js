/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/DOMIdentity.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FirefoxAccounts",
                                  "resource://gre/modules/identity/FirefoxAccounts.jsm");

// Make the profile dir available; this is necessary so that
// services/fxaccounts/FxAccounts.jsm can read and write its signed-in user
// data.
do_get_profile();

function MockFXAManager() {
  this.signedIn = true;
}
MockFXAManager.prototype = {
  getAssertion: function(audience) {
    let result = this.signedIn ? TEST_ASSERTION : null;
    return Promise.resolve(result);
  },

  signOut: function() {
    this.signedIn = false;
    return Promise.resolve(null);
  },
}

let originalManager = FirefoxAccounts.fxAccountsManager;
FirefoxAccounts.fxAccountsManager = new MockFXAManager();
do_register_cleanup(() => {
  log("restoring fxaccountsmanager");
  FirefoxAccounts.fxAccountsManager = originalManager;
});

function test_overall() {
  do_check_neq(FirefoxAccounts, null);
  run_next_test();
}

function test_mock() {
  do_test_pending();

  FirefoxAccounts.fxAccountsManager.getAssertion().then(assertion => {
    do_check_eq(assertion, TEST_ASSERTION);
    do_test_finished();
    run_next_test();
  });
}

function test_watch_signed_in() {
  do_test_pending();

  let received = [];

  let mockedRP = mock_fxa_rp(null, TEST_URL, function(method, data) {
    received.push([method, data]);

    if (method == "ready") {
      // confirm that we were signed in and then ready was called
      do_check_eq(received.length, 2);
      do_check_eq(received[0][0], "login");
      do_check_eq(received[0][1], TEST_ASSERTION);
      do_check_eq(received[1][0], "ready");
      do_test_finished();
      run_next_test();
    }
  });

  FirefoxAccounts.RP.watch(mockedRP);
}

function test_watch_signed_out() {
  do_test_pending();

  let received = [];
  let signedInState = FirefoxAccounts.fxAccountsManager.signedIn;
  FirefoxAccounts.fxAccountsManager.signedIn = false;

  let mockedRP = mock_fxa_rp(null, TEST_URL, function(method) {
    received.push(method);

    if (method == "ready") {
      // confirm that we were signed out and then ready was called
      do_check_eq(received.length, 2);
      do_check_eq(received[0], "logout");
      do_check_eq(received[1], "ready");

      // restore initial state
      FirefoxAccounts.fxAccountsManager.signedIn = signedInState;
      do_test_finished();
      run_next_test();
    }
  });

  FirefoxAccounts.RP.watch(mockedRP);
}

function test_request() {
  do_test_pending();

  let received = [];

  // initially signed out
  let signedInState = FirefoxAccounts.fxAccountsManager.signedIn;
  FirefoxAccounts.fxAccountsManager.signedIn = false;

  let mockedRP = mock_fxa_rp(null, TEST_URL, function(method, data) {
    received.push([method, data]);

    // On watch(), we are signed out.  Then we call request().
    if (received.length === 2) {
      do_check_eq(received[0][0], "logout");
      do_check_eq(received[1][0], "ready");

      // Pretend request() showed ux and the user signed in
      FirefoxAccounts.fxAccountsManager.signedIn = true;
      FirefoxAccounts.RP.request(mockedRP.id);
    }

    if (received.length === 3) {
      do_check_eq(received[2][0], "login");
      do_check_eq(received[2][1], TEST_ASSERTION);

      // restore initial state
      FirefoxAccounts.fxAccountsManager.signedIn = signedInState;
      do_test_finished();
      run_next_test();
    }
  });

  // First, call watch()
  FirefoxAccounts.RP.watch(mockedRP);
}

function test_logout() {
  do_test_pending();

  let received = [];

  let mockedRP = mock_fxa_rp(null, TEST_URL, function(method) {
    received.push(method);

    // At first, watch() signs us in automatically.  Then we sign out.
    if (received.length === 2) {
      do_check_eq(received[0], "login");
      do_check_eq(received[1], "ready");

      FirefoxAccounts.RP.logout(mockedRP.id);
    }

    if (received.length === 3) {
      do_check_eq(received[2], "logout");
      do_test_finished();
      run_next_test();
    }
  });

  // First, call watch()
  FirefoxAccounts.RP.watch(mockedRP);
}

function test_error() {
  do_test_pending();

  let received = [];

  // Mock the fxAccountsManager so that getAssertion rejects its promise and
  // triggers our onerror handler.  (This is the method that's used internally
  // by FirefoxAccounts.RP.request().)
  let originalManager = FirefoxAccounts.fxAccountsManager;
  FirefoxAccounts.RP.fxAccountsManager = {
    getAssertion: function(audience) {
      return Promise.reject(new Error("barf!"));
    }
  };

  let mockedRP = mock_fxa_rp(null, TEST_URL, function(method, message) {
    // We will immediately receive an error, due to watch()'s attempt
    // to getAssertion().
    do_check_eq(method, "error");
    do_check_true(/barf/.test(message));

    // Put things back the way they were
    FirefoxAccounts.fxAccountsManager = originalManager;

    do_test_finished();
    run_next_test();
  });

  // First, call watch()
  FirefoxAccounts.RP.watch(mockedRP);
}

function test_child_process_shutdown() {
  do_test_pending();
  let rpCount = FirefoxAccounts.RP._rpFlows.size;

  makeObserver("identity-child-process-shutdown", (aTopic, aSubject, aData) => {
    // Last of all, the shutdown observer message will be fired.
    // This takes place after the RP has a chance to delete flows
    // and clean up.
    do_check_eq(FirefoxAccounts.RP._rpFlows.size, rpCount);
    do_test_finished();
    run_next_test();
  });

  let mockedRP = mock_fxa_rp(null, TEST_URL, (method) => {
    // We should enter this function for 'ready' and 'child-process-shutdown'.
    // After we have a chance to do our thing, the shutdown observer message
    // will fire and be caught by the function above.
    do_check_eq(FirefoxAccounts.RP._rpFlows.size, rpCount + 1);
    switch (method) {
      case "ready":
        DOMIdentity._childProcessShutdown("my message manager");
        break;

      case "child-process-shutdown":
        // We have to call this explicitly because there's no real
        // dom window here.
        FirefoxAccounts.RP.childProcessShutdown(mockedRP._mm);
        break;

      default:
        break;
    }
  });

  mockedRP._mm = "my message manager";
  FirefoxAccounts.RP.watch(mockedRP);

  // fake a dom window context
  DOMIdentity.newContext(mockedRP, mockedRP._mm);
}

let TESTS = [
  test_overall,
  test_mock,
  test_watch_signed_in,
  test_watch_signed_out,
  test_request,
  test_logout,
  test_error,
  test_child_process_shutdown,
];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
