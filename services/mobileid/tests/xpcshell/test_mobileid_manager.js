/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const Cm = Components.manager;

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// We need to set the server url pref before importing MobileIdentityManager.
// Otherwise, it won't be able to import it as getting the non existing pref
// will throw.
Services.prefs.setCharPref("services.mobileid.server.uri",
                           "https://dummyurl.com");

// Set do_printging on.
Services.prefs.setCharPref("services.mobileid.loglevel", "do_print");

Cu.import("resource://gre/modules/MobileIdentityManager.jsm");
Cu.import("resource://gre/modules/MobileIdentityCommon.jsm");

const DEBUG = false;

const GET_ASSERTION_IPC_MSG = "MobileId:GetAssertion";
const GET_ASSERTION_RETURN_OK = "MobileId:GetAssertion:Return:OK";
const GET_ASSERTION_RETURN_KO = "MobileId:GetAssertion:Return:KO";

// === Globals ===

const ORIGIN = "app://afakeorigin";
const APP_ID = 1;
const PRINCIPAL = {
  origin: ORIGIN,
  appId: APP_ID
};
const PHONE_NUMBER = "+34666555444";
const ANOTHER_PHONE_NUMBER = "+44123123123";
const VERIFICATION_CODE = "123456";
const SESSION_TOKEN = "aSessionToken";
const ICC_ID = "aIccId";
const MNC = "aMnc";
const MCC = 214;
const OPERATOR = "aOperator";
const CERTIFICATE = "eyJhbGciOiJEUzI1NiJ9.eyJsYXN0QXV0aEF0IjoxNDA0NDY5NzkyODc3LCJ2ZXJpZmllZE1TSVNETiI6IiszMTYxNzgxNTc1OCIsInB1YmxpYy1rZXkiOnsiYWxnb3JpdGhtIjoiRFMiLCJ5IjoiNGE5YzkzNDY3MWZhNzQ3YmM2ZjMyNjE0YTg1MzUyZjY5NDcwMDdhNTRkMDAxMDY4OWU5ZjJjZjc0ZGUwYTEwZTRlYjlmNDk1ZGFmZTA0NGVjZmVlNDlkN2YwOGU4ODQyMDJiOTE5OGRhNWZhZWE5MGUzZjRmNzE1YzZjNGY4Yjc3MGYxZTU4YWZhNDM0NzVhYmFiN2VlZGE1MmUyNjk2YzFmNTljNzMzYjFlYzBhNGNkOTM1YWIxYzkyNzAxYjNiYTA5ZDRhM2E2MzNjNTJmZjE2NGYxMWY3OTg1YzlmZjY3ZThmZDFlYzA2NDU3MTdkMjBiNDE4YmM5M2YzYzVkNCIsInAiOiJmZjYwMDQ4M2RiNmFiZmM1YjQ1ZWFiNzg1OTRiMzUzM2Q1NTBkOWYxYmYyYTk5MmE3YThkYWE2ZGMzNGY4MDQ1YWQ0ZTZlMGM0MjlkMzM0ZWVlYWFlZmQ3ZTIzZDQ4MTBiZTAwZTRjYzE0OTJjYmEzMjViYTgxZmYyZDVhNWIzMDVhOGQxN2ViM2JmNGEwNmEzNDlkMzkyZTAwZDMyOTc0NGE1MTc5MzgwMzQ0ZTgyYTE4YzQ3OTMzNDM4Zjg5MWUyMmFlZWY4MTJkNjljOGY3NWUzMjZjYjcwZWEwMDBjM2Y3NzZkZmRiZDYwNDYzOGMyZWY3MTdmYzI2ZDAyZTE3IiwicSI6ImUyMWUwNGY5MTFkMWVkNzk5MTAwOGVjYWFiM2JmNzc1OTg0MzA5YzMiLCJnIjoiYzUyYTRhMGZmM2I3ZTYxZmRmMTg2N2NlODQxMzgzNjlhNjE1NGY0YWZhOTI5NjZlM2M4MjdlMjVjZmE2Y2Y1MDhiOTBlNWRlNDE5ZTEzMzdlMDdhMmU5ZTJhM2NkNWRlYTcwNGQxNzVmOGViZjZhZjM5N2Q2OWUxMTBiOTZhZmIxN2M3YTAzMjU5MzI5ZTQ4MjliMGQwM2JiYzc4OTZiMTViNGFkZTUzZTEzMDg1OGNjMzRkOTYyNjlhYTg5MDQxZjQwOTEzNmM3MjQyYTM4ODk1YzlkNWJjY2FkNGYzODlhZjFkN2E0YmQxMzk4YmQwNzJkZmZhODk2MjMzMzk3YSJ9LCJwcmluY2lwYWwiOiIwMzgxOTgyYS0xZTgzLTI1NjYtNjgzZS05MDRmNDA0NGM1MGRAbXNpc2RuLWRldi5zdGFnZS5tb3phd3MubmV0IiwiaWF0IjoxNDA0NDY5NzgyODc3LCJleHAiOjE0MDQ0OTEzOTI4NzcsImlzcyI6Im1zaXNkbi1kZXYuc3RhZ2UubW96YXdzLm5ldCJ9."

// === Helpers ===

function addPermission(aAction) {
  let uri = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService)
              .newURI(ORIGIN, null, null);
  let _principal = Cc["@mozilla.org/scriptsecuritymanager;1"]
                     .getService(Ci.nsIScriptSecurityManager)
                     .getAppCodebasePrincipal(uri, APP_ID, false);
  let pm = Cc["@mozilla.org/permissionmanager;1"]
             .getService(Ci.nsIPermissionManager);
  pm.addFromPrincipal(_principal, MOBILEID_PERM, aAction);
}

function removePermission() {
  let uri = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService)
              .newURI(ORIGIN, null, null);
  let _principal = Cc["@mozilla.org/scriptsecuritymanager;1"]
                     .getService(Ci.nsIScriptSecurityManager)
                     .getAppCodebasePrincipal(uri, APP_ID, false);
  let pm = Cc["@mozilla.org/permissionmanager;1"]
             .getService(Ci.nsIPermissionManager);
  pm.removeFromPrincipal(_principal, MOBILEID_PERM);
}

// === Mocks ===

let Mock = function(aOptions) {
  if (!aOptions) {
    aOptions = {};
  }
  this._options = aOptions;
  this._spied = {};
};

Mock.prototype = {
  _: function(aMethod) {
    DEBUG && do_print("_ " + aMethod + JSON.stringify(this._spied));
    let self = this;
    return {
      callsLength: function(aNumberOfCalls) {
        if (aNumberOfCalls == 0) {
          do_check_eq(self._spied[aMethod], undefined);
          return;
        }
        do_check_eq(self._spied[aMethod].length, aNumberOfCalls);
      },
      call: function(aCallNumber) {
        return {
          arg: function(aArgNumber, aValue) {
            let _arg = self._spied[aMethod][aCallNumber - 1][aArgNumber - 1];
            if (Array.isArray(aValue)) {
              do_check_eq(_arg.length, aValue.length)
              for (let i = 0; i < _arg.length; i++) {
                do_check_eq(_arg[i], aValue[i]);
              }
              return;
            }

            if (typeof aValue === 'object') {
              do_check_eq(JSON.stringify(_arg), JSON.stringify(aValue));
              return;
            }

            do_check_eq(_arg, aValue);
          }
        }
      }
    }
  },

  _spy: function(aMethod, aArgs) {
    DEBUG && do_print(aMethod + " - " + JSON.stringify(aArgs));
    if (!this._spied[aMethod]) {
      this._spied[aMethod] = [];
    }
    this._spied[aMethod].push(aArgs);
  },

  getSpiedCalls: function(aMethod) {
    return this._spied[aMethod];
  }
};

// UI Glue mock up.
let MockUi = function(aOptions) {
  Mock.call(this, aOptions);
};

MockUi.prototype = {
  __proto__: Mock.prototype,

  _startFlowResult: {
    phoneNumber: PHONE_NUMBER,
    mcc: MNC
  },

  _verifyCodePromptResult: {
    verificationCode: VERIFICATION_CODE
  },

  startFlow: function() {
    this._spy("startFlow", arguments);
    return Promise.resolve(this._options.startFlowResult ||
                           this._startFlowResult);
  },

  verificationCodePrompt: function() {
    this._spy("verifyCodePrompt", arguments);
    return Promise.resolve(this._options.verificationCodePromptResult ||
                           this._verifyCodePromptResult);
  },

  verify: function() {
    this._spy("verify", arguments);
  },

  error: function() {
    this._spy("error", arguments);
  },

  verified: function() {
    this._spy("verified", arguments);
  },

  set oncancel(aCallback) {
  },

  set onresendcode(aCallback) {
  }
};

// Save original credential store instance.
const kMobileIdentityCredStore = MobileIdentityManager.credStore;

// Credentials store mock up.
let MockCredStore = function(aOptions) {
  Mock.call(this, aOptions);
};

MockCredStore.prototype = {
  __proto__: Mock.prototype,

  _getByOriginResult: null,

  _getByMsisdnResult: null,

  _getByIccIdResult: null,

  getByOrigin: function() {
    this._spy("getByOrigin", arguments);
    let result = this._getByOriginResult;
    if (this._options.getByOriginResult) {
      if (Array.isArray(this._options.getByOriginResult)) {
        result = this._options.getByOriginResult.length ?
                 this._options.getByOriginResult.shift() : null;
      } else {
        result = this._options.getByOriginResult;
      }
    }
    return Promise.resolve(result);
  },

  getByMsisdn: function() {
    this._spy("getByMsisdn", arguments);
    return Promise.resolve(this._options.getByMsisdnResult ||
                           this._getByMsisdnResult);
  },

  getByIccId: function() {
    this._spy("getByIccId", arguments);
    return Promise.resolve(this._options.getByIccIdResult ||
                           this._getByIccIdResult);
  },

  add: function() {
    this._spy("add", arguments);
    return Promise.resolve();
  },

  setDeviceIccIds: function() {
    this._spy("setDeviceIccIds", arguments);
    return Promise.resolve();
  },

  removeOrigin: function() {
    this._spy("removeOrigin", arguments);
    return Promise.resolve();
  },

  delete: function() {
    this._spy("delete", arguments);
    return Promise.resolve();
  }
};

// Save original client instance.
const kMobileIdentityClient = MobileIdentityManager.client;

// Client mock up.
let MockClient = function(aOptions) {
  Mock.call(this, aOptions);
};

MockClient.prototype = {

  __proto__: Mock.prototype,

  _discoverResult: {
    verificationMethods: ["sms/mt"],
    verificationDetails: {
      "sms/mt": {
        mtSender: "123",
        url: "https://msisdn.accounts.firefox.com/v1/msisdn/sms/mt/verify"
      }
    }
  },

  _registerResult: {
    msisdnSessionToken: SESSION_TOKEN
  },

  _smsMtVerifyResult: {},

  _verifyCodeResult: {
    msisdn: PHONE_NUMBER
  },

  _signResult: {
    cert: CERTIFICATE
  },

  hawk: {
    now: function() {
      return Date.now();
    }
  },

  discover: function() {
    this._spy("discover", arguments);
    return Promise.resolve(this._options.discoverResult ||
                           this._discoverResult);
  },

  register: function() {
    this._spy("register", arguments);
    return Promise.resolve(this._options.registerResult ||
                           this._registerResult);
  },

  smsMtVerify: function() {
    this._spy("smsMtVerify", arguments);
    return Promise.resolve(this._options.smsMtVerifyResult ||
                           this._smsMtVerifyResult);
  },

  verifyCode: function() {
    this._spy("verifyCode", arguments);
    return Promise.resolve(this._options.verifyCodeResult ||
                           this._verifyCodeResult);
  },

  sign: function() {
    this._spy("sign", arguments);
    if (this._options.signError) {
      let error = Array.isArray(this._options.signError) ?
                  this._options.signError.shift() :
                  this._options.signError;
      return Promise.reject(error);
    }
    return Promise.resolve(this._options.signResult || this._signResult);
  }
};

// The test rely on having an app registered. Otherwise, it will throw.
// Override XULAppInfo.
const XUL_APP_INFO_UUID = Components.ID("{84fdc459-d96d-421c-9bff-a8193233ae75}");
const XUL_APP_INFO_CONTRACT_ID = "@mozilla.org/xre/app-info;1";

let (XULAppInfo = {
  vendor: "Mozilla",
  name: "MobileIdTest",
  ID: "{230de50e-4cd1-11dc-8314-0800200b9a66}",
  version: "1",
  appBuildID: "2007010101",
  platformVersion: "",
  platformBuildID: "2007010101",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIXULAppInfo,
    Ci.nsIXULRuntime,
  ])
}) {
  let XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }
      return XULAppInfo.QueryInterface(iid);
    }
  };
  Cm.QueryInterface(Ci.nsIComponentRegistrar)
    .registerFactory(XUL_APP_INFO_UUID,
                     "XULAppInfo",
                     XUL_APP_INFO_CONTRACT_ID,
                     XULAppInfoFactory);
}

// === Global cleanup ===

function cleanup() {
  MobileIdentityManager.credStore = kMobileIdentityCredStore;
  MobileIdentityManager.client = kMobileIdentityClient;
  MobileIdentityManager.ui = null;
  MobileIdentityManager.iccInfo = null;
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
      credStore._("add").call(1).arg(5, null);


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
      credStore._("add").call(1).arg(5, null);
      credStore._("setDeviceIccIds").callsLength(1);
      credStore._("setDeviceIccIds").call(1).arg(1, PHONE_NUMBER);
      credStore._("setDeviceIccIds").call(1).arg(2, null);

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
      credStore._("add").call(1).arg(5, null);
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
      credStore._("add").call(1).arg(5, null);
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
      credStore._("add").call(1).arg(5, null);
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
    deviceIccIds: null
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
      credStore._("add").call(1).arg(5, null);
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
