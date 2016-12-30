/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "IDService",
                                  "resource://gre/modules/identity/Identity.jsm",
                                  "IdentityService");

XPCOMUtils.defineLazyModuleGetter(this, "jwcrypto",
                                  "resource://gre/modules/identity/jwcrypto.jsm");

function test_begin_authentication_flow() {
  do_test_pending();
  let _provId = null;

  // set up a watch, to be consistent
  let mockedDoc = mock_doc(null, TEST_URL, function(action, params) {});
  IDService.RP.watch(mockedDoc);

  // The identity-auth notification is sent up to the UX from the
  // _doAuthentication function.  Be ready to receive it and call
  // beginAuthentication
  makeObserver("identity-auth", function(aSubject, aTopic, aData) {
    do_check_neq(aSubject, null);

    do_check_eq(aSubject.wrappedJSObject.provId, _provId);

    do_test_finished();
    run_next_test();
  });

  setup_provisioning(
    TEST_USER,
    function(caller) {
      _provId = caller.id;
      IDService.IDP.beginProvisioning(caller);
    }, function() {},
    {
      beginProvisioningCallback(email, duration_s) {

        // let's say this user needs to authenticate
        IDService.IDP._doAuthentication(_provId, {idpParams:TEST_IDPPARAMS});
      }
    }
  );
}

function test_complete_authentication_flow() {
  do_test_pending();
  let _provId = null;
  let _authId = null;
  let id = TEST_USER;

  let callbacksFired = false;
  let loginStateChanged = false;
  let identityAuthComplete = false;

  // The result of authentication should be a successful login
  IDService.reset();

  setup_test_identity(id, TEST_CERT, function() {
    // set it up so we're supposed to be logged in to TEST_URL

    get_idstore().setLoginState(TEST_URL, true, id);

    // When we authenticate, our ready callback will be fired.
    // At the same time, a separate topic will be sent up to the
    // the observer in the UI.  The test is complete when both
    // events have occurred.
    let mockedDoc = mock_doc(id, TEST_URL, call_sequentially(
      function(action, params) {
        do_check_eq(action, 'ready');
        do_check_eq(params, undefined);

        // if notification already received by observer, test is done
        callbacksFired = true;
        if (loginStateChanged && identityAuthComplete) {
          do_test_finished();
          run_next_test();
        }
      }
    ));

    makeObserver("identity-auth-complete", function(aSubject, aTopic, aData) {
      identityAuthComplete = true;
      do_test_finished();
      run_next_test();
    });

    makeObserver("identity-login-state-changed", function(aSubject, aTopic, aData) {
      do_check_neq(aSubject, null);

      do_check_eq(aSubject.wrappedJSObject.rpId, mockedDoc.id);
      do_check_eq(aData, id);

      // if callbacks in caller doc already fired, test is done.
      loginStateChanged = true;
      if (callbacksFired && identityAuthComplete) {
        do_test_finished();
        run_next_test();
      }
    });

    IDService.RP.watch(mockedDoc);

    // Create a provisioning flow for our auth flow to attach to
    setup_provisioning(
      TEST_USER,
      function(provFlow) {
        _provId = provFlow.id;

        IDService.IDP.beginProvisioning(provFlow);
      }, function() {},
      {
        beginProvisioningCallback(email, duration_s) {
          // let's say this user needs to authenticate
          IDService.IDP._doAuthentication(_provId, {idpParams:TEST_IDPPARAMS});

          // test_begin_authentication_flow verifies that the right
          // message is sent to the UI.  So that works.  Moving on,
          // the UI calls setAuthenticationFlow ...
          _authId = uuid();
          IDService.IDP.setAuthenticationFlow(_authId, _provId);

          // ... then the UI calls beginAuthentication ...
          authCaller.id = _authId;
          IDService.IDP._provisionFlows[_provId].caller = authCaller;
          IDService.IDP.beginAuthentication(authCaller);
        }
      }
    );
  });

  // A mock calling context
  let authCaller = {
    doBeginAuthenticationCallback: function doBeginAuthenticationCallback(identity) {
      do_check_eq(identity, TEST_USER);
      // completeAuthentication will emit "identity-auth-complete"
      IDService.IDP.completeAuthentication(_authId);
    },

    doError(err) {
      log("OW! My doError callback hurts!", err);
    },
  };

}

var TESTS = [];

TESTS.push(test_begin_authentication_flow);
TESTS.push(test_complete_authentication_flow);

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
