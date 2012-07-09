/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * By their nature, these tests duplicate some of the functionality of
 * other tests for Identity, RelyingParty, and IdentityProvider.
 *
 * In particular, "identity-auth-complete" and
 * "identity-login-state-changed" are tested in test_authentication.js
 */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "IDService",
                                  "resource:///modules/identity/Identity.jsm",
                                  "IdentityService");

function test_smoke() {
  do_check_neq(IDService, null);
  run_next_test();
}

function test_identity_request() {
  // In response to navigator.id.request(), initiate a login with user
  // interaction by notifying observers of 'identity-request'

  do_test_pending();

  IDService.reset();

  let id = "landru@mockmyid.com";
  setup_test_identity(id, TEST_CERT, function() {
    // deliberately adding a trailing final slash on the domain
    // to test path composition
    let mockedDoc = mock_doc(null, "http://jed.gov/", function() {});

    // by calling watch() we create an rp flow.
    IDService.RP.watch(mockedDoc);

    // register the request UX observer
    makeObserver("identity-request", function (aSubject, aTopic, aData) {
      do_check_eq(aTopic, "identity-request");
      do_check_eq(aData, null);

      // check that all the URLs are properly resolved
      let subj = aSubject.wrappedJSObject;
      do_check_eq(subj.privacyPolicy, "http://jed.gov/pp.html");
      do_check_eq(subj.termsOfService, "http://jed.gov/tos.html");

      do_test_finished();
      run_next_test();
    });

    let requestOptions = {
      privacyPolicy: "/pp.html",
      termsOfService: "/tos.html"
    };
    IDService.RP.request(mockedDoc.id, requestOptions);
  });

}

function test_identity_auth() {
  // see test_authentication.js for "identity-auth-complete"
  // and "identity-login-state-changed"

  do_test_pending();
  let _provId = "bogus";

  // Simulate what would be returned by IDService._fetchWellKnownFile
  // for a given domain.
  let idpParams = {
    domain: "myfavoriteflan.com",
    idpParams: {
      authentication: "/foo/authenticate.html",
      provisioning: "/foo/provision.html"
    }
  };

  // Create an RP flow
  let mockedDoc = mock_doc(null, TEST_URL, function(action, params) {});
  IDService.RP.watch(mockedDoc);

  // The identity-auth notification is sent up to the UX from the
  // _doAuthentication function.  Be ready to receive it and call
  // beginAuthentication
  makeObserver("identity-auth", function (aSubject, aTopic, aData) {
    do_check_neq(aSubject, null);
    do_check_eq(aTopic, "identity-auth");
    do_check_eq(aData, "https://myfavoriteflan.com/foo/authenticate.html");

    do_check_eq(aSubject.wrappedJSObject.provId, _provId);
    do_test_finished();
    run_next_test();
  });

  // Even though our provisioning flow id is bogus, IdentityProvider
  // won't look at it until farther along in the authentication
  // process.  So this test can pass with a fake provId.
  IDService.IDP._doAuthentication(_provId, idpParams);
}

let TESTS = [
    test_smoke,
    test_identity_request,
    test_identity_auth,
  ];


TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
