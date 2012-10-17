"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "MinimalIDService",
                                  "resource://gre/modules/identity/MinimalIdentity.jsm",
                                  "IdentityService");

Cu.import("resource://gre/modules/identity/LogUtils.jsm");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["test_minimalidentity"].concat(aMessageArgs));
}

function test_overall() {
  do_check_neq(MinimalIDService, null);
  run_next_test();
}

function test_mock_doc() {
  do_test_pending();
  let mockedDoc = mock_doc(null, TEST_URL, function(action, params) {
    do_check_eq(action, 'coffee');
    do_test_finished();
    run_next_test();
  });

  mockedDoc.doCoffee();
}

/*
 * Test that the "identity-controller-watch" signal is emitted correctly
 */
function test_watch() {
  do_test_pending();

  let mockedDoc = mock_doc(null, TEST_URL);
  makeObserver("identity-controller-watch", function (aSubject, aTopic, aData) {
    do_check_eq(aSubject.wrappedJSObject.rpId, mockedDoc.id);
    do_check_eq(aSubject.wrappedJSObject.origin, TEST_URL);
    do_test_finished();
    run_next_test();
   });

  MinimalIDService.RP.watch(mockedDoc);
}

/*
 * Test that the "identity-controller-request" signal is emitted correctly
 */
function test_request() {
  do_test_pending();

  let mockedDoc = mock_doc(null, TEST_URL);
  makeObserver("identity-controller-request", function (aSubject, aTopic, aData) {
    do_check_eq(aSubject.wrappedJSObject.rpId, mockedDoc.id);
    do_check_eq(aSubject.wrappedJSObject.origin, TEST_URL);
    do_test_finished();
    run_next_test();
  });

  MinimalIDService.RP.watch(mockedDoc);
  MinimalIDService.RP.request(mockedDoc.id, {});
}

/*
 * Test that the "identity-controller-logout" signal is emitted correctly
 */
function test_logout() {
  do_test_pending();

  let mockedDoc = mock_doc(null, TEST_URL);
  makeObserver("identity-controller-logout", function (aSubject, aTopic, aData) {
    do_check_eq(aSubject.wrappedJSObject.rpId, mockedDoc.id);
    do_test_finished();
    run_next_test();
  });

  MinimalIDService.RP.watch(mockedDoc);
  MinimalIDService.RP.logout(mockedDoc.id, {});
}



let TESTS = [
  test_overall,
  test_mock_doc,
  test_watch,
  test_request,
  test_logout
];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}