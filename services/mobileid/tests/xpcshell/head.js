/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

var Cm = Components.manager;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

(function initMobileIdTestingInfrastructure() {
  do_get_profile();

  const PREF_FORCE_HTTPS = "services.mobileid.forcehttps";
  Services.prefs.setBoolPref(PREF_FORCE_HTTPS, false);
  Services.prefs.setCharPref("services.mobileid.loglevel", "Debug");
  Services.prefs.setCharPref("services.mobileid.server.uri",
                             "https://dummyurl.com");
}).call(this);

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
const ANOTHER_ICC_ID = "anotherIccId";
const MNC = "aMnc";
const ANOTHER_MNC = "anotherMnc";
const MCC = "aMcc";
const ANOTHER_MCC = "anotherMcc";
const OPERATOR = "aOperator";
const ANOTHER_OPERATOR = "anotherOperator";
const ICC_INFO = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIGsmIccInfo,
                                         Ci.nsIIccInfo]),
  iccType: "usim",
  iccid: ICC_ID,
  mcc: MCC,
  mnc: MNC,
  msisdn: PHONE_NUMBER,
  operator: OPERATOR
};
const ANOTHER_ICC_INFO = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIGsmIccInfo,
                                         Ci.nsIIccInfo]),
  iccType: "usim",
  iccid: ANOTHER_ICC_ID,
  mcc: ANOTHER_MCC,
  mnc: ANOTHER_MNC,
  msisdn: ANOTHER_PHONE_NUMBER,
  operator: ANOTHER_OPERATOR
};
const INVALID_ICC_INFO = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIGsmIccInfo,
                                         Ci.nsIIccInfo]),
  iccType: "usim",
  iccid: null,
  mcc: "",
  mnc: "",
  msisdn: "",
  operator: ""
};
const RADIO_INTERFACE = {
  voice: {
    network: {
      shortName: OPERATOR
    },
    roaming: false
  },
  data: {
    network: {
      shortName: OPERATOR
    }
  }
};
const ANOTHER_RADIO_INTERFACE = {
  voice: {
    network: {
      shortName: ANOTHER_OPERATOR
    },
    roaming: false
  },
  data: {
    network: {
      shortName: ANOTHER_OPERATOR
    }
  }
};

const INVALID_RADIO_INTERFACE = {
  voice: {
    network: {
      shortName: ""
    },
    roaming: undefined
  },
  data: {
    network: {
      shortName: ""
    }
  }
};

const CERTIFICATE = "eyJhbGciOiJEUzI1NiJ9.eyJsYXN0QXV0aEF0IjoxNDA0NDY5NzkyODc3LCJ2ZXJpZmllZE1TSVNETiI6IiszMTYxNzgxNTc1OCIsInB1YmxpYy1rZXkiOnsiYWxnb3JpdGhtIjoiRFMiLCJ5IjoiNGE5YzkzNDY3MWZhNzQ3YmM2ZjMyNjE0YTg1MzUyZjY5NDcwMDdhNTRkMDAxMDY4OWU5ZjJjZjc0ZGUwYTEwZTRlYjlmNDk1ZGFmZTA0NGVjZmVlNDlkN2YwOGU4ODQyMDJiOTE5OGRhNWZhZWE5MGUzZjRmNzE1YzZjNGY4Yjc3MGYxZTU4YWZhNDM0NzVhYmFiN2VlZGE1MmUyNjk2YzFmNTljNzMzYjFlYzBhNGNkOTM1YWIxYzkyNzAxYjNiYTA5ZDRhM2E2MzNjNTJmZjE2NGYxMWY3OTg1YzlmZjY3ZThmZDFlYzA2NDU3MTdkMjBiNDE4YmM5M2YzYzVkNCIsInAiOiJmZjYwMDQ4M2RiNmFiZmM1YjQ1ZWFiNzg1OTRiMzUzM2Q1NTBkOWYxYmYyYTk5MmE3YThkYWE2ZGMzNGY4MDQ1YWQ0ZTZlMGM0MjlkMzM0ZWVlYWFlZmQ3ZTIzZDQ4MTBiZTAwZTRjYzE0OTJjYmEzMjViYTgxZmYyZDVhNWIzMDVhOGQxN2ViM2JmNGEwNmEzNDlkMzkyZTAwZDMyOTc0NGE1MTc5MzgwMzQ0ZTgyYTE4YzQ3OTMzNDM4Zjg5MWUyMmFlZWY4MTJkNjljOGY3NWUzMjZjYjcwZWEwMDBjM2Y3NzZkZmRiZDYwNDYzOGMyZWY3MTdmYzI2ZDAyZTE3IiwicSI6ImUyMWUwNGY5MTFkMWVkNzk5MTAwOGVjYWFiM2JmNzc1OTg0MzA5YzMiLCJnIjoiYzUyYTRhMGZmM2I3ZTYxZmRmMTg2N2NlODQxMzgzNjlhNjE1NGY0YWZhOTI5NjZlM2M4MjdlMjVjZmE2Y2Y1MDhiOTBlNWRlNDE5ZTEzMzdlMDdhMmU5ZTJhM2NkNWRlYTcwNGQxNzVmOGViZjZhZjM5N2Q2OWUxMTBiOTZhZmIxN2M3YTAzMjU5MzI5ZTQ4MjliMGQwM2JiYzc4OTZiMTViNGFkZTUzZTEzMDg1OGNjMzRkOTYyNjlhYTg5MDQxZjQwOTEzNmM3MjQyYTM4ODk1YzlkNWJjY2FkNGYzODlhZjFkN2E0YmQxMzk4YmQwNzJkZmZhODk2MjMzMzk3YSJ9LCJwcmluY2lwYWwiOiIwMzgxOTgyYS0xZTgzLTI1NjYtNjgzZS05MDRmNDA0NGM1MGRAbXNpc2RuLWRldi5zdGFnZS5tb3phd3MubmV0IiwiaWF0IjoxNDA0NDY5NzgyODc3LCJleHAiOjE0MDQ0OTEzOTI4NzcsImlzcyI6Im1zaXNkbi1kZXYuc3RhZ2UubW96YXdzLm5ldCJ9."

// === Helpers ===

function addPermission(aAction) {
  let uri = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService)
              .newURI(ORIGIN, null, null);
  let attrs = {appId: APP_ID};
  let _principal = Cc["@mozilla.org/scriptsecuritymanager;1"]
                     .getService(Ci.nsIScriptSecurityManager)
                     .createCodebasePrincipal(uri, attrs);
  let pm = Cc["@mozilla.org/permissionmanager;1"]
             .getService(Ci.nsIPermissionManager);
  pm.addFromPrincipal(_principal, MOBILEID_PERM, aAction);
}

function removePermission() {
  let uri = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService)
              .newURI(ORIGIN, null, null);
  let attrs = {appId: APP_ID};
  let _principal = Cc["@mozilla.org/scriptsecuritymanager;1"]
                     .getService(Ci.nsIScriptSecurityManager)
                     .createCodebasePrincipal(uri, attrs);
  let pm = Cc["@mozilla.org/permissionmanager;1"]
             .getService(Ci.nsIPermissionManager);
  pm.removeFromPrincipal(_principal, MOBILEID_PERM);
}

// === Mocks ===

var Mock = function(aOptions) {
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
var MockUi = function(aOptions) {
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

// Credentials store mock up.
var MockCredStore = function(aOptions) {
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

// Client mock up.
var MockClient = function(aOptions) {
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
    if (this._options.verifyCodeError) {
      let error = Array.isArray(this._options.verifyCodeError) ?
                  this._options.verifyCodeError.shift() :
                  this._options.verifyCodeError;
      if (!this._options.verifyCodeError.length) {
        this._options.verifyCodeError = null;
      }
      return Promise.reject(error);
    }
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

// Override MobileIdentityUIGlue.
const kMobileIdentityUIGlueUUID = "{05df0566-ca8a-4ec7-bc76-78626ebfbe9a}";
const kMobileIdentityUIGlueContractID =
  "@mozilla.org/services/mobileid-ui-glue;1";

// Save original factory.
/*const kMobileIdentityUIGlueFactory =
  Cm.getClassObject(Cc[kMobileIdentityUIGlueContractID], Ci.nsIFactory);*/

var fakeMobileIdentityUIGlueFactory = {
  createInstance: function(aOuter, aIid) {
    return MobileIdentityUIGlue.QueryInterface(aIid);
  }
};

// MobileIdentityUIGlue fake component.
var MobileIdentityUIGlue = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileIdentityUIGlue]),

};

(function registerFakeMobileIdentityUIGlue() {
  Cm.QueryInterface(Ci.nsIComponentRegistrar)
    .registerFactory(Components.ID(kMobileIdentityUIGlueUUID),
                     "MobileIdentityUIGlue",
                     kMobileIdentityUIGlueContractID,
                     fakeMobileIdentityUIGlueFactory);
})();

// The tests rely on having an app registered. Otherwise, it will throw.
// Override XULAppInfo.
const XUL_APP_INFO_UUID = Components.ID("{84fdc459-d96d-421c-9bff-a8193233ae75}");
const XUL_APP_INFO_CONTRACT_ID = "@mozilla.org/xre/app-info;1";

var XULAppInfo = {
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
};

var XULAppInfoFactory = {
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
