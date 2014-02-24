/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const Cm = Components.manager;

Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsManager.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

// === Mocks ===

// Override FxAccountsUIGlue.
const kFxAccountsUIGlueUUID = "{8f6d5d87-41ed-4bb5-aa28-625de57564c5}";
const kFxAccountsUIGlueContractID =
  "@mozilla.org/fxaccounts/fxaccounts-ui-glue;1";

// Save original FxAccountsUIGlue factory.
const kFxAccountsUIGlueFactory =
  Cm.getClassObject(Cc[kFxAccountsUIGlueContractID], Ci.nsIFactory);

let fakeFxAccountsUIGlueFactory = {
  createInstance: function(aOuter, aIid) {
    return FxAccountsUIGlue.QueryInterface(aIid);
  }
};

// FxAccountsUIGlue fake component.
let FxAccountsUIGlue = {
  _reject: false,

  _error: 'error',

  _signInFlowCalled: false,

  _refreshAuthCalled: false,

  _activeSession: null,

  _reset: function() {
    this._reject = false;
    this._error = 'error';
    this._signInFlowCalled = false;
    this._refreshAuthCalled = false;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFxAccountsUIGlue]),

  _promise: function() {
    let deferred = Promise.defer();

    if (this._reject) {
      deferred.reject(this._error);
    } else {
      FxAccountsManager._activeSession = this._activeSession || {
        email: "user@domain.org",
        verified: false,
        sessionToken: "1234"
      };
      FxAccountsManager._fxAccounts
                       .setSignedInUser(FxAccountsManager._activeSession);
      deferred.resolve(FxAccountsManager._activeSession);
    }

    return deferred.promise;
  },

  signInFlow: function() {
    this._signInFlowCalled = true;
    return this._promise();
  },

  refreshAuthentication: function() {
    this._refreshAuthCalled = true;
    return this._promise();
  }
};

(function registerFakeFxAccountsUIGlue() {
  Cm.QueryInterface(Ci.nsIComponentRegistrar)
    .registerFactory(Components.ID(kFxAccountsUIGlueUUID),
                     "FxAccountsUIGlue",
                     kFxAccountsUIGlueContractID,
                     fakeFxAccountsUIGlueFactory);
})();

// Save original fxAccounts instance
const kFxAccounts = fxAccounts;
// and change it for a mock FxAccounts.
FxAccountsManager._fxAccounts = {
  _reject: false,
  _getSignedInUserCalled: false,
  _setSignedInUserCalled: false,

  _error: 'error',
  _assertion: 'assertion',
  _signedInUser: null,

  _reset: function() {
    this._getSignedInUserCalled = false;
    this._setSignedInUserCalled = false;
    this._reject = false;
  },

  getAssertion: function() {
    if (!this._signedInUser) {
      return null;
    }

    let deferred = Promise.defer();
    deferred.resolve(this._assertion);
    return deferred.promise;
  },

  getSignedInUser: function() {
    this._getSignedInUserCalled = true;
    let deferred = Promise.defer();
    this._reject ? deferred.reject(this._error)
                 : deferred.resolve(this._signedInUser);
    return deferred.promise;
  },

  setSignedInUser: function(user) {
    this._setSignedInUserCalled = true;
    let deferred = Promise.defer();
    this._signedInUser = user;
    deferred.resolve();
    return deferred.promise;
  },

  signOut: function() {
    let deferred = Promise.defer();
    this._signedInUser = null;
    Services.obs.notifyObservers(null, ONLOGOUT_NOTIFICATION, null);
    deferred.resolve();
    return deferred.promise;
  }
};

// Save original FxAccountsClient factory from FxAccountsManager.
const kFxAccountsClient = FxAccountsManager._getFxAccountsClient;

// and change it for a fake client factory.
let FakeFxAccountsClient = {
  _reject: false,
  _recoveryEmailStatusCalled: false,
  _signInCalled: false,
  _signUpCalled: false,
  _signOutCalled: false,

  _accountExists: false,
  _verified: false,
  _password: null,

  _reset: function() {
    this._reject = false;
    this._recoveryEmailStatusCalled = false;
    this._signInCalled = false;
    this._signUpCalled = false;
    this._signOutCalled = false;
  },

  recoveryEmailStatus: function() {
    this._recoveryEmailStatusCalled = true;
    let deferred = Promise.defer();
    this._reject ? deferred.reject()
                 : deferred.resolve({ verified: this._verified });
    return deferred.promise;
  },

  signIn: function(user, password) {
    this._signInCalled = true;
    this._password = password;
    let deferred = Promise.defer();
    this._reject ? deferred.reject()
                 : deferred.resolve({ email: user,
                                      uid: "whatever",
                                      verified: this._verified,
                                      sessionToken: "1234" });
    return deferred.promise;
  },

  signUp: function(user, password) {
    this._signUpCalled = true;
    return this.signIn(user, password);
  },

  signOut: function() {
    this._signOutCalled = true;
    let deferred = Promise.defer();
    this._reject ? deferred.reject()
                 : deferred.resolve();
    return deferred.promise;
  },

  accountExists: function() {
    let deferred = Promise.defer();
    this._reject ? deferred.reject()
                 : deferred.resolve(this._accountExists);
    return deferred.promise;
  }
};

FxAccountsManager._getFxAccountsClient = function() {
  return FakeFxAccountsClient;
};


// === Global cleanup ===

// Unregister mocks and restore original code.
do_register_cleanup(function() {
  // Unregister the factory so we do not leak
  Cm.QueryInterface(Ci.nsIComponentRegistrar)
    .unregisterFactory(Components.ID(kFxAccountsUIGlueUUID),
                       fakeFxAccountsUIGlueFactory);

  // Restore the original factory.
  Cm.QueryInterface(Ci.nsIComponentRegistrar)
    .registerFactory(Components.ID(kFxAccountsUIGlueUUID),
                     "FxAccountsUIGlue",
                     kFxAccountsUIGlueContractID,
                     kFxAccountsUIGlueFactory);

  // Restore the original FxAccounts instance from FxAccountsManager.
  FxAccountsManager._fxAccounts = kFxAccounts;

  // Restore the FxAccountsClient getter from FxAccountsManager.
  FxAccountsManager._getFxAccountsClient = kFxAccountsClient;
});


// === Tests ===

function run_test() {
  run_next_test();
}

add_test(function test_initial_state() {
  do_print("= Initial state =");
  do_check_neq(FxAccountsManager, undefined);
  do_check_null(FxAccountsManager._activeSession);
  do_check_null(FxAccountsManager._user);
  run_next_test();
});

add_test(function(test_getAccount_no_session) {
  do_print("= getAccount no session =");
  FxAccountsManager.getAccount().then(
    result => {
      do_check_null(result);
      do_check_null(FxAccountsManager._activeSession);
      do_check_null(FxAccountsManager._user);
      do_check_true(FxAccountsManager._fxAccounts._getSignedInUserCalled);
      FxAccountsManager._fxAccounts._reset();
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_getAssertion_no_audience) {
  do_print("= getAssertion no audience =");
  FxAccountsManager.getAssertion().then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_INVALID_AUDIENCE);
      run_next_test();
    }
  );
});

add_test(function(test_getAssertion_no_session_ui_error) {
  do_print("= getAssertion no session, UI error =");
  FxAccountsUIGlue._reject = true;
  FxAccountsManager.getAssertion("audience").then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_UI_ERROR);
      do_check_eq(error.details, "error");
      FxAccountsUIGlue._reset();
      run_next_test();
    }
  );
});

add_test(function(test_getAssertion_no_session_ui_success) {
  do_print("= getAssertion no session, UI success =");
  FxAccountsManager.getAssertion("audience").then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_true(FxAccountsUIGlue._signInFlowCalled);
      do_check_eq(error.error, ERROR_UNVERIFIED_ACCOUNT);
      FxAccountsUIGlue._reset();
      run_next_test();
    }
  );
});

add_test(function(test_getAssertion_active_session_unverified_account) {
  do_print("= getAssertion active session, unverified account =");
  FxAccountsManager.getAssertion("audience").then(
    result => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_false(FxAccountsUIGlue._signInFlowCalled);
      do_check_eq(error.error, ERROR_UNVERIFIED_ACCOUNT);
      run_next_test();
    }
  );
});

add_test(function(test_getAssertion_active_session_verified_account) {
  do_print("= getAssertion active session, verified account =");
  FxAccountsManager._fxAccounts._signedInUser.verified = true;
  FxAccountsManager._activeSession.verified = true;
  FxAccountsManager.getAssertion("audience").then(
    result => {
      do_check_false(FxAccountsUIGlue._signInFlowCalled);
      do_check_eq(result, "assertion");
      FxAccountsManager._fxAccounts._reset();
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_getAssertion_refreshAuth) {
  do_print("= getAssertion refreshAuth =");
  let gracePeriod = 1200;
  FxAccountsUIGlue._activeSession = {
    email: "user@domain.org",
    verified: true,
    sessionToken: "1234"
  };
  FxAccountsManager._fxAccounts._signedInUser.verified = true;
  FxAccountsManager._activeSession.verified = true;
  FxAccountsManager._activeSession.authAt =
    (Date.now() / 1000) - gracePeriod;
  FxAccountsManager.getAssertion("audience", {
    "refreshAuthentication": gracePeriod
  }).then(
    result => {
      do_check_false(FxAccountsUIGlue._signInFlowCalled);
      do_check_true(FxAccountsUIGlue._refreshAuthCalled);
      do_check_eq(result, "assertion");
      FxAccountsManager._fxAccounts._reset();
      FxAccountsUIGlue._reset();
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_getAssertion_refreshAuth_NaN) {
  do_print("= getAssertion refreshAuth NaN=");
  let gracePeriod = "NaN";
  FxAccountsManager.getAssertion("audience", {
    "refreshAuthentication": gracePeriod
  }).then(
    result => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_false(FxAccountsUIGlue._signInFlowCalled);
      do_check_false(FxAccountsUIGlue._refreshAuthCalled);
      do_check_eq(error.error, ERROR_INVALID_REFRESH_AUTH_VALUE);
      FxAccountsManager._fxAccounts._reset();
      run_next_test();
    }
  );
});

add_test(function(test_getAssertion_refresh_auth_no_refresh) {
  do_print("= getAssertion refreshAuth no refresh =");
  FxAccountsManager._fxAccounts._signedInUser.verified = true;
  FxAccountsManager._activeSession.verified = true;
  FxAccountsManager._activeSession.authAt =
    (Date.now() / 1000) + 10000;
  FxAccountsManager.getAssertion("audience", {
    "refreshAuthentication": 1
  }).then(
    result => {
      do_check_false(FxAccountsUIGlue._signInFlowCalled);
      do_check_eq(result, "assertion");
      FxAccountsManager._fxAccounts._reset();
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_getAccount_existing_verified_session) {
  do_print("= getAccount, existing verified session =");
  FxAccountsManager.getAccount().then(
    result => {
      do_check_false(FxAccountsManager._fxAccounts._getSignedInUserCalled);
      do_check_eq(result.accountId, FxAccountsManager._user.accountId);
      do_check_eq(result.verified, FxAccountsManager._user.verified);
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_getAccount_existing_unverified_session_unverified_user) {
  do_print("= getAccount, existing unverified session, unverified user =");
  FxAccountsManager._activeSession.verified = false;
  FxAccountsManager._fxAccounts._signedInUser.verified = false;
  FxAccountsManager.getAccount().then(
    result => {
      do_check_true(FakeFxAccountsClient._recoveryEmailStatusCalled);
      do_check_false(result.verified);
      do_check_eq(result.accountId, FxAccountsManager._user.accountId);
      FakeFxAccountsClient._reset();
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_getAccount_existing_unverified_session_verified_user) {
  do_print("= getAccount, existing unverified session, verified user =");
  FxAccountsManager._activeSession.verified = false;
  FxAccountsManager._fxAccounts._signedInUser.verified = false;
  FakeFxAccountsClient._verified = true;
  FxAccountsManager.getAccount().then(
    result => {
      do_check_true(FakeFxAccountsClient._recoveryEmailStatusCalled);
      do_check_true(result.verified);
      do_check_eq(result.accountId, FxAccountsManager._user.accountId);
      FakeFxAccountsClient._reset();
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_signOut) {
  do_print("= signOut =");
  do_check_true(FxAccountsManager._activeSession != null);
  FxAccountsManager.signOut().then(
    result => {
      do_check_null(result);
      do_check_null(FxAccountsManager._activeSession);
      do_check_true(FakeFxAccountsClient._signOutCalled);
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_verificationStatus_no_token_session) {
  do_print("= verificationStatus, no token session =");
  do_check_null(FxAccountsManager._activeSession);
  FxAccountsManager.verificationStatus().then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_NO_TOKEN_SESSION);
      run_next_test();
    }
  );
});

add_test(function(test_signUp_no_accountId) {
  do_print("= signUp, no accountId=");
  FxAccountsManager.signUp().then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_INVALID_ACCOUNTID);
      run_next_test();
    }
  );
});

add_test(function(test_signIn_no_accountId) {
  do_print("= signIn, no accountId=");
  FxAccountsManager.signIn().then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_INVALID_ACCOUNTID);
      run_next_test();
    }
  );
});

add_test(function(test_signUp_no_password) {
  do_print("= signUp, no accountId=");
  FxAccountsManager.signUp("user@domain.org").then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_INVALID_PASSWORD);
      run_next_test();
    }
  );
});

add_test(function(test_signIn_no_accountId) {
  do_print("= signIn, no accountId=");
  FxAccountsManager.signIn("user@domain.org").then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_INVALID_PASSWORD);
      run_next_test();
    }
  );
});

add_test(function(test_signUp) {
  do_print("= signUp =");
  FakeFxAccountsClient._verified = false;
  FxAccountsManager.signUp("user@domain.org", "password").then(
    result => {
      do_check_true(FakeFxAccountsClient._signInCalled);
      do_check_true(FakeFxAccountsClient._signUpCalled);
      do_check_true(FxAccountsManager._fxAccounts._getSignedInUserCalled);
      do_check_eq(FxAccountsManager._fxAccounts._signedInUser.email, "user@domain.org");
      do_check_eq(FakeFxAccountsClient._password, "password");
      do_check_true(result.accountCreated);
      do_check_eq(result.user.accountId, "user@domain.org");
      do_check_false(result.user.verified);
      FakeFxAccountsClient._reset();
      FxAccountsManager._fxAccounts._reset();
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error.error);
    }
  );
});

add_test(function(test_signUp_already_signed_user) {
  do_print("= signUp, already signed user =");
  FxAccountsManager.signUp("user@domain.org", "password").then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_false(FakeFxAccountsClient._signInCalled);
      do_check_eq(error.error, ERROR_ALREADY_SIGNED_IN_USER);
      do_check_eq(error.details.user.accountId, "user@domain.org");
      do_check_false(error.details.user.verified);
      run_next_test();
    }
  );
});

add_test(function(test_signIn_already_signed_user) {
  do_print("= signIn, already signed user =");
  FxAccountsManager.signIn("user@domain.org", "password").then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_ALREADY_SIGNED_IN_USER);
      do_check_eq(error.details.user.accountId, "user@domain.org");
      do_check_false(error.details.user.verified);
      run_next_test();
    }
  );
});

add_test(function(test_verificationStatus_unverified_session_unverified_user) {
  do_print("= verificationStatus unverified session and user =");
  FakeFxAccountsClient._verified = false;
  FxAccountsManager.verificationStatus().then(
    user => {
      do_check_false(user.verified);
      do_check_true(FakeFxAccountsClient._recoveryEmailStatusCalled);
      do_check_false(FxAccountsManager._fxAccounts._setSignedInUserCalled);
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_verificationStatus_unverified_session_verified_user) {
  do_print("= verificationStatus unverified session, verified user =");
  FakeFxAccountsClient._verified = true;
  FxAccountsManager.verificationStatus().then(
    user => {
      do_check_true(user.verified);
      do_check_true(FakeFxAccountsClient._recoveryEmailStatusCalled);
      do_check_true(FxAccountsManager._fxAccounts._setSignedInUserCalled);
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_queryAccount_no_exists) {
  do_print("= queryAccount, no exists =");
  FxAccountsManager.queryAccount("user@domain.org").then(
    result => {
      do_check_false(result.registered);
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_queryAccount_exists) {
  do_print("= queryAccount, exists =");
  FakeFxAccountsClient._accountExists = true;
  FxAccountsManager.queryAccount("user@domain.org").then(
    result => {
      do_check_true(result.registered);
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_queryAccount_no_accountId) {
  do_print("= queryAccount, no accountId =");
  FxAccountsManager.queryAccount().then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_INVALID_ACCOUNTID);
      run_next_test();
    }
  );
});

add_test(function() {
  do_print("= fxaccounts:onlogout notification =");
  do_check_true(FxAccountsManager._activeSession != null);
  Services.obs.notifyObservers(null, ONLOGOUT_NOTIFICATION, null);
  do_execute_soon(function() {
    do_check_null(FxAccountsManager._activeSession);
    run_next_test();
  });
});
