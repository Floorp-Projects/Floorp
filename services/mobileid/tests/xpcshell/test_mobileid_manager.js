/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/MobileIdentityManager.jsm");
Cu.import("resource://gre/modules/MobileIdentityCommon.jsm");

// Save original credential store instance.
const kMobileIdentityCredStore = MobileIdentityManager.credStore;
// Save original client instance.
const kMobileIdentityClient = MobileIdentityManager.client;

// === Global cleanup ===
function cleanup() {
  MobileIdentityManager.credStore = kMobileIdentityCredStore;
  MobileIdentityManager.client = kMobileIdentityClient;
  MobileIdentityManager.ui = null;
  MobileIdentityManager._iccInfo = [];
  removePermission(ORIGIN);
}

// Unregister mocks and restore original code.
do_register_cleanup(cleanup);
// === Tests ===
function run_test() {
  run_next_test();
}

add_test(function() {
  do_print("= Initial state =");
  do_check_neq(MobileIdentityManager, undefined);
  run_next_test();
});

add_test(function() {
  do_print("= No credentials - No ICC - User MSISDN - External - OK result =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let ui = new MockUi();
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore();
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient();
  MobileIdentityManager.client = client;
  MobileIdentityManager._iccInfo = [];

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_OK);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(1);
      credStore._("getByOrigin").call(1).arg(1, ORIGIN);
      credStore._("getByMsisdn").callsLength(1);
      credStore._("getByMsisdn").call(1).arg(1, PHONE_NUMBER);
      credStore._("add").callsLength(1);
      credStore._("add").call(1).arg(1, undefined);
      credStore._("add").call(1).arg(2, PHONE_NUMBER);
      credStore._("add").call(1).arg(3, ORIGIN);
      credStore._("add").call(1).arg(4, SESSION_TOKEN);
      credStore._("add").call(1).arg(5, []);


      // MockUI.
      ui._("startFlow").callsLength(1);
      ui._("startFlow").call(1).arg(1, "");
      ui._("startFlow").call(1).arg(2, []);
      ui._("verifyCodePrompt").callsLength(1);
      ui._("verifyCodePrompt").call(1).arg(1, 3);
      ui._("verify").callsLength(1);

      // MockClient.
      client._("discover").callsLength(1);
      client._("discover").call(1).arg(1, PHONE_NUMBER);
      client._("register").callsLength(1);
      client._("smsMtVerify").callsLength(1);
      client._("smsMtVerify").call(1).arg(1, SESSION_TOKEN);
      client._("smsMtVerify").call(1).arg(2, PHONE_NUMBER);
      client._("smsMtVerify").call(1).arg(3, MNC);
      client._("smsMtVerify").call(1).arg(4, undefined);
      client._("smsMtVerify").call(1).arg(5, true);
      client._("verifyCode").callsLength(1);
      client._("verifyCode").call(1).arg(1, SESSION_TOKEN);
      client._("verifyCode").call(1).arg(2, {
        verificationCode: VERIFICATION_CODE
      });
      client._("sign").callsLength(1);
      client._("sign").call(1).arg(1, SESSION_TOKEN);
      client._("sign").call(1).arg(2, CERTIFICATE_LIFETIME);

      do_test_finished();
      run_next_test();
    }
  };

  addPermission(Ci.nsIPermissionManager.ALLOW_ACTION);

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId
    }
  });
});

add_test(function() {
  do_print("= No credentials - No icc - User MSISDN - External - KO -" +
           " ERROR_INTERNAL_CANNOT_VERIFY_SELECTION =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let _sessionToken = Date.now();

  let ui = new MockUi();
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore();
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient({
    discoverResult: {
      verificationMethods: ["other"],
      verificationDetails: {
        "other": {}
      }
    },
    registerResult: {
      msisdnSessionToken: _sessionToken
    }
  });
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_KO);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);
      do_check_eq(aData.error, ERROR_INTERNAL_CANNOT_VERIFY_SELECTION);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(1);
      credStore._("getByOrigin").call(1).arg(1, ORIGIN);
      credStore._("getByMsisdn").callsLength(1);
      credStore._("getByMsisdn").call(1).arg(1, PHONE_NUMBER);
      credStore._("add").callsLength(0);

      // MockUI.
      ui._("startFlow").callsLength(1);
      ui._("startFlow").call(1).arg(1, "");
      ui._("startFlow").call(1).arg(2, []);
      ui._("verifyCodePrompt").callsLength(0);
      ui._("verify").callsLength(0);
      ui._("error").callsLength(1);
      ui._("error").call(1).arg(1, ERROR_INTERNAL_CANNOT_VERIFY_SELECTION);

      // MockClient.
      client._("discover").callsLength(1);
      client._("discover").call(1).arg(1, PHONE_NUMBER);
      client._("register").callsLength(0);
      client._("smsMtVerify").callsLength(0);
      client._("verifyCode").callsLength(0);
      client._("sign").callsLength(0);

      do_test_finished();
      run_next_test();
    }
  };

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId
    }
  });
});

add_test(function() {
  do_print("= No credentials - No icc - User MSISDN - External - KO -" +
           " INTERNAL_INVALID_PROMPT_RESULT =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let _sessionToken = Date.now();

  let ui = new MockUi({
    startFlowResult: {}
  });
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore();
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient();
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_KO);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);
      do_check_eq(aData.error, ERROR_INTERNAL_INVALID_PROMPT_RESULT);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(1);
      credStore._("getByOrigin").call(1).arg(1, ORIGIN);
      credStore._("getByMsisdn").callsLength(0);
      credStore._("add").callsLength(0);

      // MockUI.
      ui._("startFlow").callsLength(1);
      ui._("startFlow").call(1).arg(1, "");
      ui._("startFlow").call(1).arg(2, []);
      ui._("verifyCodePrompt").callsLength(0);
      ui._("verify").callsLength(0);
      ui._("error").callsLength(1);
      ui._("error").call(1).arg(1, ERROR_INTERNAL_INVALID_PROMPT_RESULT);

      // MockClient.
      client._("discover").callsLength(0);
      client._("register").callsLength(0);
      client._("smsMtVerify").callsLength(0);
      client._("verifyCode").callsLength(0);
      client._("sign").callsLength(0);

      do_test_finished();
      run_next_test();
    }
  };

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId
    }
  });
});

add_test(function() {
  do_print("= No credentials - No icc - User MSISDN - External - KO -" +
           " ERROR_INVALID_ASSERTION =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let _sessionToken = Date.now();

  let ui = new MockUi();
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore();
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient({
    signResult: {
      cert: "aInvalidCert"
    },
    registerResult: {
      msisdnSessionToken: _sessionToken
    }
  });
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_KO);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);
      do_check_eq(aData.error, ERROR_INVALID_ASSERTION);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(1);
      credStore._("getByOrigin").call(1).arg(1, ORIGIN);
      credStore._("getByMsisdn").callsLength(1);
      credStore._("getByMsisdn").call(1).arg(1, PHONE_NUMBER);
      credStore._("add").callsLength(1);

      // MockUI.
      ui._("startFlow").callsLength(1);
      ui._("startFlow").call(1).arg(1, "");
      ui._("startFlow").call(1).arg(2, []);
      ui._("verifyCodePrompt").callsLength(1);
      ui._("verifyCodePrompt").call(1).arg(1, 3);
      ui._("verify").callsLength(1);
      ui._("error").callsLength(1);
      ui._("error").call(1).arg(1, ERROR_INVALID_ASSERTION);

      // MockClient.
      client._("discover").callsLength(1);
      client._("discover").call(1).arg(1, PHONE_NUMBER);
      client._("register").callsLength(1);
      client._("smsMtVerify").callsLength(1);
      client._("smsMtVerify").call(1).arg(1, _sessionToken);
      client._("smsMtVerify").call(1).arg(2, PHONE_NUMBER);
      client._("smsMtVerify").call(1).arg(3, MNC);
      client._("smsMtVerify").call(1).arg(4, undefined);
      client._("smsMtVerify").call(1).arg(5, true);
      client._("verifyCode").callsLength(1);
      client._("verifyCode").call(1).arg(1, _sessionToken);
      client._("verifyCode").call(1).arg(2, {
        verificationCode: VERIFICATION_CODE
      });
      client._("sign").callsLength(1);
      client._("sign").call(1).arg(1, _sessionToken);
      client._("sign").call(1).arg(2, CERTIFICATE_LIFETIME);

      do_test_finished();
      run_next_test();
    }
  };

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId
    }
  });
});

add_test(function() {
  do_print("= Existing credentials - No Icc - Permission - OK result =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let ui = new MockUi();
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore({
    getByOriginResult: {
      sessionToken: SESSION_TOKEN,
      msisdn: PHONE_NUMBER,
      origin: ORIGIN,
      deviceIccIds: null
    }
  });
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient();
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_OK);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(1);
      credStore._("getByOrigin").call(1).arg(1, ORIGIN);

      removePermission(ORIGIN);

      do_test_finished();
      run_next_test();
    }
  };

  addPermission(Ci.nsIPermissionManager.ALLOW_ACTION);

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId,
      options: {}
    }
  });
});

add_test(function() {
  do_print("= Existing credentials - No Icc - Prompt permission - OK result =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let ui = new MockUi();
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore({
    getByOriginResult: {
      sessionToken: SESSION_TOKEN,
      msisdn: PHONE_NUMBER,
      origin: ORIGIN,
      deviceIccIds: null
    }
  });
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient();
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_OK);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(1);
      credStore._("getByOrigin").call(1).arg(1, ORIGIN);

      // MockUI.
      ui._("startFlow").callsLength(1);

      removePermission(ORIGIN);

      do_test_finished();
      run_next_test();
    }
  };

  addPermission(Ci.nsIPermissionManager.PROMPT_ACTION);

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId,
      options: {}
    }
  });
});

add_test(function() {
  do_print("= Existing credentials - No Icc - Permission denied - KO result =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let ui = new MockUi();
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore({
    getByOriginResult: {
      sessionToken: SESSION_TOKEN,
      msisdn: PHONE_NUMBER,
      origin: ORIGIN,
      deviceIccIds: null
    }
  });
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient();
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_KO);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);
      do_check_eq(aData.error, ERROR_PERMISSION_DENIED);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(0);

      // MockUI.
      ui._("startFlow").callsLength(0);
      ui._("error").callsLength(0);

      do_test_finished();
      run_next_test();
    }
  };

  removePermission(ORIGIN);

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId,
      options: {}
    }
  });
});

add_test(function() {
  do_print("= Existing credentials - No Icc - SIM change/Same choice - " +
           "OK result =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let ui = new MockUi();
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore({
    getByOriginResult: {
      sessionToken: SESSION_TOKEN,
      msisdn: PHONE_NUMBER,
      origin: ORIGIN,
      deviceIccIds: [ICC_ID]
    }
  });
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient();
  MobileIdentityManager.client = client;

  MobileIdentityManager._iccInfo = [];

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_OK);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(1);
      credStore._("getByOrigin").call(1).arg(1, ORIGIN);
      credStore._("add").callsLength(1);
      credStore._("add").call(1).arg(1, undefined);
      credStore._("add").call(1).arg(2, PHONE_NUMBER);
      credStore._("add").call(1).arg(3, ORIGIN);
      credStore._("add").call(1).arg(4, SESSION_TOKEN);
      credStore._("add").call(1).arg(5, []);
      credStore._("setDeviceIccIds").callsLength(1);
      credStore._("setDeviceIccIds").call(1).arg(1, PHONE_NUMBER);
      credStore._("setDeviceIccIds").call(1).arg(2, []);

      // MockUI.
      ui._("startFlow").callsLength(1);
      ui._("verifyCodePrompt").callsLength(0);
      ui._("verify").callsLength(0);

      // MockClient.
      client._("discover").callsLength(0);
      client._("register").callsLength(0);
      client._("smsMtVerify").callsLength(0);
      client._("verifyCode").callsLength(0);
      client._("sign").callsLength(0);

      do_test_finished();
      run_next_test();
    }
  };

  addPermission(Ci.nsIPermissionManager.ALLOW_ACTION);

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId,
      options: {}
    }
  });
});

add_test(function() {
  do_print("= Existing credentials - No Icc - SIM change/Different choice - " +
           "OK result =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let _sessionToken = Date.now();

  let existingCredentials = {
    sessionToken: SESSION_TOKEN,
    msisdn: PHONE_NUMBER,
    origin: ORIGIN,
    deviceIccIds: [ICC_ID]
  };

  let ui = new MockUi({
    startFlowResult: {
      phoneNumber: ANOTHER_PHONE_NUMBER
    }
  });
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore({
    getByOriginResult: existingCredentials
  });
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient({
    verifyCodeResult: ANOTHER_PHONE_NUMBER,
    registerResult: {
      msisdnSessionToken: _sessionToken
    }
  });
  MobileIdentityManager.client = client;

  MobileIdentityManager._iccInfo = [];

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_OK);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(1);
      credStore._("getByOrigin").call(1).arg(1, ORIGIN);
      credStore._("getByMsisdn").callsLength(1);
      credStore._("getByMsisdn").call(1).arg(1, ANOTHER_PHONE_NUMBER);
      credStore._("add").callsLength(1);
      credStore._("add").call(1).arg(1, undefined);
      credStore._("add").call(1).arg(2, ANOTHER_PHONE_NUMBER);
      credStore._("add").call(1).arg(3, ORIGIN);
      credStore._("add").call(1).arg(4, _sessionToken);
      credStore._("add").call(1).arg(5, []);
      credStore._("setDeviceIccIds").callsLength(0);
      credStore._("removeOrigin").callsLength(1);
      credStore._("removeOrigin").call(1).arg(1, PHONE_NUMBER);
      credStore._("removeOrigin").call(1).arg(2, ORIGIN);

      // MockUI.
      ui._("startFlow").callsLength(1);
      ui._("verifyCodePrompt").callsLength(1);
      ui._("verify").callsLength(1);

      // MockClient.
      client._("discover").callsLength(1);
      client._("register").callsLength(1);
      client._("smsMtVerify").callsLength(1);
      client._("verifyCode").callsLength(1);
      client._("sign").callsLength(1);

      do_test_finished();
      run_next_test();
    }
  };

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId,
      options: {}
    }
  });
});

add_test(function() {
  do_print("= Existing credentials - No Icc - forceSelection/same - " +
           "OK result =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let _sessionToken = Date.now();

  let existingCredentials = {
    sessionToken: _sessionToken,
    msisdn: PHONE_NUMBER,
    origin: ORIGIN,
    deviceIccIds: []
  };

  let ui = new MockUi({
    startFlowResult: {
      phoneNumber: PHONE_NUMBER
    }
  });
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore({
    getByOriginResult: existingCredentials
  });
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient();
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_OK);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(1);
      credStore._("getByOrigin").call(1).arg(1, ORIGIN);
      credStore._("getByMsisdn").callsLength(0);
      credStore._("add").callsLength(1);
      credStore._("add").call(1).arg(1, undefined);
      credStore._("add").call(1).arg(2, PHONE_NUMBER);
      credStore._("add").call(1).arg(3, ORIGIN);
      credStore._("add").call(1).arg(4, _sessionToken);
      credStore._("add").call(1).arg(5, []);
      credStore._("setDeviceIccIds").callsLength(1);
      credStore._("removeOrigin").callsLength(0);

      // MockUI.
      ui._("startFlow").callsLength(1);
      ui._("verifyCodePrompt").callsLength(0);
      ui._("verify").callsLength(0);

      // MockClient.
      client._("discover").callsLength(0);
      client._("register").callsLength(0);
      client._("smsMtVerify").callsLength(0);
      client._("verifyCode").callsLength(0);
      client._("sign").callsLength(1);

      do_test_finished();
      run_next_test();
    }
  };

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId,
      options: {
        forceSelection: true
      }
    }
  });
});
add_test(function() {
  do_print("= Existing credentials - No Icc - forceSelection/different - " +
           "OK result =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let _sessionToken = Date.now();

  let existingCredentials = {
    sessionToken: SESSION_TOKEN,
    msisdn: PHONE_NUMBER,
    origin: ORIGIN,
    deviceIccIds: []
  };

  let ui = new MockUi({
    startFlowResult: {
      phoneNumber: ANOTHER_PHONE_NUMBER
    }
  });
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore({
    getByOriginResult: existingCredentials
  });
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient({
    verifyCodeResult: ANOTHER_PHONE_NUMBER,
    registerResult: {
      msisdnSessionToken: _sessionToken
    }
  });
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_OK);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(1);
      credStore._("getByOrigin").call(1).arg(1, ORIGIN);
      credStore._("getByMsisdn").callsLength(1);
      credStore._("getByMsisdn").call(1).arg(1, ANOTHER_PHONE_NUMBER);
      credStore._("add").callsLength(1);
      credStore._("add").call(1).arg(1, undefined);
      credStore._("add").call(1).arg(2, ANOTHER_PHONE_NUMBER);
      credStore._("add").call(1).arg(3, ORIGIN);
      credStore._("add").call(1).arg(4, _sessionToken);
      credStore._("add").call(1).arg(5, []);
      credStore._("setDeviceIccIds").callsLength(0);
      credStore._("removeOrigin").callsLength(1);
      credStore._("removeOrigin").call(1).arg(1, PHONE_NUMBER);
      credStore._("removeOrigin").call(1).arg(2, ORIGIN);

      // MockUI.
      ui._("startFlow").callsLength(1);
      ui._("verifyCodePrompt").callsLength(1);
      ui._("verify").callsLength(1);

      // MockClient.
      client._("discover").callsLength(1);
      client._("register").callsLength(1);
      client._("smsMtVerify").callsLength(1);
      client._("verifyCode").callsLength(1);
      client._("sign").callsLength(1);

      do_test_finished();
      run_next_test();
    }
  };

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId,
      options: {
        forceSelection: true
      }
    }
  });
});

add_test(function() {
  do_print("= Existing credentials - No Icc - INVALID_AUTH_TOKEN - OK =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let _sessionToken = Date.now();

  let existingCredentials = {
    sessionToken: _sessionToken,
    msisdn: PHONE_NUMBER,
    origin: ORIGIN,
    deviceIccIds: []
  };

  let ui = new MockUi({
    startFlowResult: {
      phoneNumber: PHONE_NUMBER
    }
  });
  MobileIdentityManager.ui = ui;
  let credStore = new MockCredStore({
    getByOriginResult: [existingCredentials, null]
  });
  MobileIdentityManager.credStore = credStore;
  let client = new MockClient({
    signError: [ERROR_INVALID_AUTH_TOKEN],
    verifyCodeResult: PHONE_NUMBER,
    registerResult: {
      msisdnSessionToken: SESSION_TOKEN
    }
  });
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_OK);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);

      // Check spied calls.

      // MockCredStore.
      credStore._("getByOrigin").callsLength(2);
      credStore._("getByOrigin").call(1).arg(1, ORIGIN);
      credStore._("getByOrigin").call(2).arg(1, ORIGIN);
      credStore._("getByMsisdn").callsLength(1);
      credStore._("getByMsisdn").call(1).arg(1, PHONE_NUMBER);
      credStore._("add").callsLength(1);
      credStore._("add").call(1).arg(1, undefined);
      credStore._("add").call(1).arg(2, PHONE_NUMBER);
      credStore._("add").call(1).arg(3, ORIGIN);
      credStore._("add").call(1).arg(4, SESSION_TOKEN);
      credStore._("add").call(1).arg(5, []);
      credStore._("setDeviceIccIds").callsLength(0);
      credStore._("delete").callsLength(1);
      credStore._("delete").call(1).arg(1, PHONE_NUMBER);

      // MockUI.
      ui._("startFlow").callsLength(1);
      ui._("verifyCodePrompt").callsLength(1);
      ui._("verify").callsLength(1);

      // MockClient.
      client._("discover").callsLength(1);
      client._("register").callsLength(1);
      client._("smsMtVerify").callsLength(1);
      client._("verifyCode").callsLength(1);
      client._("sign").callsLength(1);

      do_test_finished();
      run_next_test();
    }
  };

  addPermission(Ci.nsIPermissionManager.ALLOW_ACTION);

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId,
      options: {}
    }
  });
});

add_test(function() {
  do_print("= ICC info change =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let _sessionToken = Date.now();

  MobileIdentityManager._iccInfo = null;
  MobileIdentityManager._iccIds = null;

  MobileIdentityManager._ril = {
    _interfaces: [RADIO_INTERFACE, ANOTHER_RADIO_INTERFACE],
    get numRadioInterfaces() {
      return this._interfaces.length;
    },

    getRadioInterface: function(aIndex) {
      return this._interfaces[aIndex];
    }
  };

  MobileIdentityManager._mobileConnectionService = {
    _interfaces: [RADIO_INTERFACE, ANOTHER_RADIO_INTERFACE],
    getItemByServiceId: function(aIndex) {
      return this._interfaces[aIndex];
    }
  };

  MobileIdentityManager._iccService = {
    _listeners: [],
    _iccInfos: [ICC_INFO, ANOTHER_ICC_INFO],
    getIccByServiceId: function(aClientId) {
      let self = this;
      return {
        get iccInfo() {
          return self._iccInfos[aClientId];
        },
        registerListener: function(aIccListener) {
          self._listeners.push(aIccListener);
        },
        unregisterListener: function() {
          self._listeners.pop();
        }
      };
    }
  };

  let ui = new MockUi();
  ui.startFlow = function() {
    // At this point we've already built the ICC cache.
    let mockIccInfo = [ICC_INFO, ANOTHER_ICC_INFO];
    for (let i = 0; i < mockIccInfo.length; i++) {
      let mIdIccInfo = MobileIdentityManager._iccInfo[i];
      do_check_eq(mockIccInfo[i].iccid, mIdIccInfo.iccId);
      do_check_eq(mockIccInfo[i].mcc, mIdIccInfo.mcc);
      do_check_eq(mockIccInfo[i].mnc, mIdIccInfo.mnc);
      do_check_eq(mockIccInfo[i].msisdn, mIdIccInfo.msisdn);
      do_check_eq(mockIccInfo[i].operator, mIdIccInfo.operator);
    }

    // We should have listeners for each valid icc.
    do_check_eq(MobileIdentityManager._iccService._listeners.length, 2);

    // We can mock an ICC change event at this point.
    MobileIdentityManager._iccService._listeners[0].notifyIccInfoChanged();

    // After the ICC change event the caches should be null.
    do_check_null(MobileIdentityManager._iccInfo);
    do_check_null(MobileIdentityManager._iccIds);

    // And we should have unregistered all listeners for ICC change events.
    do_check_eq(MobileIdentityManager._iccService._listeners.length, 0);

    do_test_finished();
    run_next_test();
  };
  MobileIdentityManager.ui = ui;

  let credStore = new MockCredStore();
  credStore.getByOrigin = function() {
    // Initially the ICC caches should be null.
    do_check_null(MobileIdentityManager._iccInfo);
    do_check_null(MobileIdentityManager._iccIds);
    return Promise.resolve(null);
  };
  MobileIdentityManager.credStore = credStore;

  let client = new MockClient();
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function() {}
  };

  addPermission(Ci.nsIPermissionManager.ALLOW_ACTION);

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId,
      options: {}
    }
  });
});

add_test(function() {
  do_print("= Invalid ICC Info =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let _sessionToken = Date.now();

  MobileIdentityManager._iccInfo = null;
  MobileIdentityManager._iccIds = null;

  MobileIdentityManager._ril = {
    _interfaces: [INVALID_RADIO_INTERFACE],
    get numRadioInterfaces() {
      return this._interfaces.length;
    },

    getRadioInterface: function(aIndex) {
      return this._interfaces[aIndex];
    }
  };

  MobileIdentityManager._mobileConnectionService = {
    _interfaces: [INVALID_RADIO_INTERFACE],
    getItemByServiceId: function(aIndex) {
      return this._interfaces[aIndex];
    }
  };

  MobileIdentityManager._iccService = {
    _listeners: [],
    _iccInfos: [INVALID_ICC_INFO],
    getIccByServiceId: function(aClientId) {
      let self = this;
      return {
        get iccInfo() {
          return self._iccInfos[aClientId];
        },
        registerListener: function(aIccListener) {
          self._listeners.push(aIccListener);
        },
        unregisterListener: function() {
          self._listeners.pop();
        }
      };
    }
  };

  let ui = new MockUi();
  ui.startFlow = function() {
    // At this point we've already built the ICC cache.
    do_check_eq(MobileIdentityManager._iccInfo.length, 0);
    do_check_eq(MobileIdentityManager._iccIds.length, 0);

    // We should have listeners for each valid icc.
    do_check_eq(MobileIdentityManager._iccService._listeners.length, 0);

    do_test_finished();
    run_next_test();
  };
  MobileIdentityManager.ui = ui;

  let credStore = new MockCredStore();
  credStore.getByOrigin = function() {
    // Initially the ICC caches should be null.
    do_check_null(MobileIdentityManager._iccInfo);
    do_check_null(MobileIdentityManager._iccIds);
    return Promise.resolve(null);
  };
  MobileIdentityManager.credStore = credStore;

  let client = new MockClient();
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function() {}
  };

  addPermission(Ci.nsIPermissionManager.ALLOW_ACTION);

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId,
      options: {}
    }
  });
});

add_test(function() {
  do_print("= Cancel verification flow =");

  do_register_cleanup(cleanup);

  do_test_pending();

  let _sessionToken = Date.now();

  let ui = new MockUi();
  ui.verificationCodePrompt = function() {
    MobileIdentityManager.onUICancel();
  };
  MobileIdentityManager.ui = ui;

  let credStore = new MockCredStore();
  MobileIdentityManager.credStore = credStore;

  let client = new MockClient();
  MobileIdentityManager.client = client;

  let promiseId = Date.now();
  let mm = {
    sendAsyncMessage: function(aMsg, aData) {
      do_print("sendAsyncMessage " + aMsg + " - " + JSON.stringify(aData));

      // Check result.
      do_check_eq(aMsg, GET_ASSERTION_RETURN_KO);
      do_check_eq(typeof aData, "object");
      do_check_eq(aData.promiseId, promiseId);
      do_check_eq(aData.error, DIALOG_CLOSED_BY_USER);

      do_test_finished();
      run_next_test();
    }
  };

  addPermission(Ci.nsIPermissionManager.ALLOW_ACTION);

  MobileIdentityManager.receiveMessage({
    name: GET_ASSERTION_IPC_MSG,
    principal: PRINCIPAL,
    target: mm,
    json: {
      promiseId: promiseId,
      options: {}
    }
  });
});

