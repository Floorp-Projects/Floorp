/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "MinimalIDService",
                                  "resource://gre/modules/identity/MinimalIdentity.jsm",
                                  "IdentityService");

Cu.import("resource://gre/modules/identity/LogUtils.jsm");
Cu.import("resource://gre/modules/DOMIdentity.jsm");

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
    do_check_eq(aSubject.wrappedJSObject.id, mockedDoc.id);
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
    do_check_eq(aSubject.wrappedJSObject.id, mockedDoc.id);
    do_check_eq(aSubject.wrappedJSObject.origin, TEST_URL);
    do_test_finished();
    run_next_test();
  });

  MinimalIDService.RP.watch(mockedDoc);
  MinimalIDService.RP.request(mockedDoc.id, {});
}

/*
 * Test that the forceAuthentication flag can be sent
 */
function test_request_forceAuthentication() {
  do_test_pending();

  let mockedDoc = mock_doc(null, TEST_URL);
  makeObserver("identity-controller-request", function (aSubject, aTopic, aData) {
    do_check_eq(aSubject.wrappedJSObject.id, mockedDoc.id);
    do_check_eq(aSubject.wrappedJSObject.origin, TEST_URL);
    do_check_eq(aSubject.wrappedJSObject.forceAuthentication, true);
    do_test_finished();
    run_next_test();
   });

  MinimalIDService.RP.watch(mockedDoc);
  MinimalIDService.RP.request(mockedDoc.id, {forceAuthentication: true});
}

/*
 * Test that the issuer can be forced
 */
function test_request_forceIssuer() {
  do_test_pending();

  let mockedDoc = mock_doc(null, TEST_URL);
  makeObserver("identity-controller-request", function (aSubject, aTopic, aData) {
    do_check_eq(aSubject.wrappedJSObject.id, mockedDoc.id);
    do_check_eq(aSubject.wrappedJSObject.origin, TEST_URL);
    do_check_eq(aSubject.wrappedJSObject.issuer, "https://jed.gov");
    do_test_finished();
    run_next_test();
   });

  MinimalIDService.RP.watch(mockedDoc);
  MinimalIDService.RP.request(mockedDoc.id, {issuer: "https://jed.gov"});
}

/*
 * Test that the "identity-controller-logout" signal is emitted correctly
 */
function test_logout() {
  do_test_pending();

  let mockedDoc = mock_doc(null, TEST_URL);
  makeObserver("identity-controller-logout", function (aSubject, aTopic, aData) {
    do_check_eq(aSubject.wrappedJSObject.id, mockedDoc.id);
    do_test_finished();
    run_next_test();
  });

  MinimalIDService.RP.watch(mockedDoc);
  MinimalIDService.RP.logout(mockedDoc.id, {});
}

/*
 * Test that logout() before watch() fails gently
 */

function test_logoutBeforeWatch() {
  do_test_pending();

  let mockedDoc = mock_doc(null, TEST_URL);
  makeObserver("identity-controller-logout", function() {
    do_throw("How can we logout when watch was not called?");
  });

  MinimalIDService.RP.logout(mockedDoc.id, {});
  do_test_finished();
  run_next_test();
}

/*
 * Test that request() before watch() fails gently
 */

function test_requestBeforeWatch() {
  do_test_pending();

  let mockedDoc = mock_doc(null, TEST_URL);
  makeObserver("identity-controller-request", function() {
    do_throw("How can we request when watch was not called?");
  });

  MinimalIDService.RP.request(mockedDoc.id, {});
  do_test_finished();
  run_next_test();
}

/*
 * Test that internal unwatch() before watch() fails gently
 */

function test_unwatchBeforeWatch() {
  do_test_pending();

  let mockedDoc = mock_doc(null, TEST_URL);

  MinimalIDService.RP.unwatch(mockedDoc.id, {});
  do_test_finished();
  run_next_test();
}

/*
 * Test that the RP flow is cleaned up on child process shutdown
 */

function test_childProcessShutdown() {
  do_test_pending();
  let UNIQUE_MESSAGE_MANAGER = "i am a beautiful snowflake";
  let initialRPCount = Object.keys(MinimalIDService.RP._rpFlows).length;

  let mockedDoc = mock_doc(null, TEST_URL, (action, params) => {
    if (action == "child-process-shutdown") {
      // since there's no actual dom window connection, we have to
      // do this bit manually here.
      MinimalIDService.RP.childProcessShutdown(UNIQUE_MESSAGE_MANAGER);
    }
  });
  mockedDoc._mm = UNIQUE_MESSAGE_MANAGER;

  makeObserver("identity-controller-watch", function (aSubject, aTopic, aData) {
    DOMIdentity._childProcessShutdown(UNIQUE_MESSAGE_MANAGER);
  });

  makeObserver("identity-child-process-shutdown", (aTopic, aSubject, aData) => {
    do_check_eq(Object.keys(MinimalIDService.RP._rpFlows).length, initialRPCount);
    do_test_finished();
    run_next_test();
  });

  // fake a dom window context
  DOMIdentity.newContext(mockedDoc, UNIQUE_MESSAGE_MANAGER);

  MinimalIDService.RP.watch(mockedDoc);
}

let TESTS = [
  test_overall,
  test_mock_doc,
  test_watch,
  test_request,
  test_request_forceAuthentication,
  test_request_forceIssuer,
  test_logout,
  test_logoutBeforeWatch,
  test_requestBeforeWatch,
  test_unwatchBeforeWatch,
  test_childProcessShutdown,
];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
