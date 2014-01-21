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

function MockFXAManager() {}
MockFXAManager.prototype = {
  getAssertion: function(audience) {
    let deferred = Promise.defer();
    deferred.resolve(TEST_ASSERTION);
    return deferred.promise;
  }
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

function test_watch() {
  do_test_pending();

  let mockedRP = mock_fxa_rp(null, TEST_URL, function(method) {
    do_check_eq(method, "ready");
    do_test_finished();
    run_next_test();
  });

  FirefoxAccounts.RP.watch(mockedRP);
}

function test_request() {
  do_test_pending();

  let received = [];

  let mockedRP = mock_fxa_rp(null, TEST_URL, function(method) {
    // We will received "ready" as a result of watch(), then "login"
    // as a result of request()
    received.push(method);

    if (received.length == 2) {
      do_check_eq(received[0], "ready");
      do_check_eq(received[1], "login");
      do_test_finished();
      run_next_test();
    }

    // Second, call request()
    if (method == "ready") {
      FirefoxAccounts.RP.request(mockedRP.id);
    }
  });

  // First, call watch()
  FirefoxAccounts.RP.watch(mockedRP);
}

function test_logout() {
  do_test_pending();

  let received = [];

  let mockedRP = mock_fxa_rp(null, TEST_URL, function(method) {
    // We will receive "ready" as a result of watch(), and "logout"
    // as a result of logout()
    received.push(method);

    if (received.length == 2) {
      do_check_eq(received[0], "ready");
      do_check_eq(received[1], "logout");
      do_test_finished();
      run_next_test();
    }

    if (method == "ready") {
      // Second, call logout()
      FirefoxAccounts.RP.logout(mockedRP.id);
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
      return Promise.reject("barf!");
    }
  };

  let mockedRP = mock_fxa_rp(null, TEST_URL, function(method, message) {
    // We will receive "ready" as a result of watch(), and "logout"
    // as a result of logout()
    received.push([method, message]);

    if (received.length == 2) {
      do_check_eq(received[0][0], "ready");

      do_check_eq(received[1][0], "error");
      do_check_eq(received[1][1], "barf!");

      // Put things back the way they were
      FirefoxAccounts.fxAccountsManager = originalManager;

      do_test_finished();
      run_next_test();
    }

    if (method == "ready") {
      FirefoxAccounts.RP.request(mockedRP.id);
    }
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
  test_watch,
  test_request,
  test_logout,
  test_error,
  test_child_process_shutdown,
];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
