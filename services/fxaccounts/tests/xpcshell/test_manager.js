/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cm = Components.manager;

Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsManager.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://testing-common/MockRegistrar.jsm");

// === Mocks ===

// Globals representing server state
var passwordResetOnServer = false;
var deletedOnServer = false;

// Global representing FxAccounts state
var certExpired = false;

// Mock RP
function makePrincipal(origin, appId) {
  let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"]
                 .getService(Ci.nsIScriptSecurityManager);
  let uri = Services.io.newURI(origin, null, null);
  return secMan.createCodebasePrincipal(uri, {appId: appId});
}
var principal = makePrincipal('app://settings.gaiamobile.org', 27, false);

// For override FxAccountsUIGlue.
var fakeFxAccountsUIGlueCID;

// FxAccountsUIGlue fake component.
var FxAccountsUIGlue = {
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
      passwordResetOnServer = false;
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
    deletedOnServer = false;
    this._signInFlowCalled = true;
    return this._promise();
  },

  refreshAuthentication: function() {
    this._refreshAuthCalled = true;
    return this._promise();
  }
};

(function registerFakeFxAccountsUIGlue() {
  fakeFxAccountsUIGlueCID =
    MockRegistrar.register("@mozilla.org/fxaccounts/fxaccounts-ui-glue;1",
                           FxAccountsUIGlue);
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
  _keys: 'keys',
  _signedInUser: null,

  _reset: function() {
    this._getSignedInUserCalled = false;
    this._setSignedInUserCalled = false;
    this._reject = false;
  },

  accountStatus: function() {
    let deferred = Promise.defer();
    deferred.resolve(!deletedOnServer);
    return deferred.promise;
  },

  getAssertion: function() {
    if (!this._signedInUser) {
      return null;
    }

    let deferred = Promise.defer();
    if (passwordResetOnServer || deletedOnServer) {
      deferred.reject({errno: ERRNO_INVALID_AUTH_TOKEN});
    } else if (Services.io.offline && certExpired) {
      deferred.reject(new Error(ERROR_OFFLINE));
    } else {
      deferred.resolve(this._assertion);
    }
    return deferred.promise;
  },

  getSignedInUser: function() {
    this._getSignedInUserCalled = true;
    let deferred = Promise.defer();
    this._reject ? deferred.reject(this._error)
                 : deferred.resolve(this._signedInUser);
    return deferred.promise;
  },

  getKeys: function() {
    let deferred = Promise.defer();
    this._reject ? deferred.reject(this._error)
                 : deferred.resolve(this._keys);
    return deferred.promise;
  },

  resendVerificationEmail: function() {
    return this.getSignedInUser().then(data => {
      if (data) {
        return Promise.resolve(true);
      }
      throw new Error("Cannot resend verification email; no signed-in user");
    });
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
var FakeFxAccountsClient = {
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
  MockRegistrar.unregister(fakeFxAccountsUIGlueCID);

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
  FxAccountsManager.getAssertion("audience", principal).then(
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
  FxAccountsManager.getAssertion("audience", principal).then(
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
  FxAccountsManager.getAssertion("audience", principal).then(
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
  FxAccountsManager.getAssertion("audience", principal).then(
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

add_test(function() {
  // getAssertion() succeeds if offline with valid cert
  do_print("= getAssertion active session, valid cert, offline");
  FxAccountsManager._fxAccounts._signedInUser.verified = true;
  FxAccountsManager._activeSession.verified = true;
  Services.io.offline = true;
  FxAccountsManager.getAssertion("audience", principal).then(
    result => {
      FxAccountsManager._fxAccounts._reset();
      Services.io.offline = false;
      run_next_test();
    },
    error => {
      Services.io.offline = false;
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function() {
  // getAssertion() rejects if offline and cert expired.
  do_print("= getAssertion active session, expired cert, offline");
  FxAccountsManager._fxAccounts._signedInUser.verified = true;
  FxAccountsManager._activeSession.verified = true;
  Services.io.offline = true;
  certExpired = true;
  FxAccountsManager.getAssertion("audience", principal).then(
    result => {
      Services.io.offline = false;
      certExpired = false;
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_OFFLINE);
      FxAccountsManager._fxAccounts._reset();
      Services.io.offline = false;
      certExpired = false;
      run_next_test();
    }
  );
});

add_test(function() {
  // getAssertion() rejects if offline and UI needed.
  do_print("= getAssertion active session, trigger UI, offline");
  let user = FxAccountsManager._fxAccounts._signedInUser;
  FxAccountsManager._fxAccounts._signedInUser = null;
  Services.io.offline = true;
  FxAccountsManager.getAssertion("audience", principal).then(
    result => {
      Services.io.offline = false;
      do_throw("Unexpected success");
    },
    error => {
      do_check_false(FxAccountsUIGlue._signInFlowCalled);
      FxAccountsManager._fxAccounts._reset();
      FxAccountsManager._fxAccounts._signedInUser = user;
      Services.io.offline = false;
      run_next_test();
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
  FxAccountsManager.getAssertion("audience", principal, {
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

add_test(function(test_getAssertion_no_permissions) {
  do_print("= getAssertion no permissions =");

  let noPermissionsPrincipal = makePrincipal('app://dummy', 28);
  let permMan = Cc["@mozilla.org/permissionmanager;1"]
                  .getService(Ci.nsIPermissionManager);
  permMan.addFromPrincipal(noPermissionsPrincipal, FXACCOUNTS_PERMISSION,
                           Ci.nsIPermissionManager.DENY_ACTION);

  FxAccountsUIGlue._activeSession = {
    email: "user@domain.org",
    verified: true,
    sessionToken: "1234"
  };

  FxAccountsManager.getAssertion("audience", noPermissionsPrincipal).then(
    result => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_false(FxAccountsUIGlue._signInFlowCalled);
      do_check_false(FxAccountsUIGlue._refreshAuthCalled);
      FxAccountsManager._fxAccounts._reset();
      FxAccountsUIGlue._reset();
      run_next_test();
    }
  );
});

add_test(function(test_getAssertion_permission_prompt_action) {
  do_print("= getAssertion PROMPT_ACTION permission =");

  let promptPermissionsPrincipal = makePrincipal('app://dummy-prompt', 29);
  let permMan = Cc["@mozilla.org/permissionmanager;1"]
                  .getService(Ci.nsIPermissionManager);
  permMan.addFromPrincipal(promptPermissionsPrincipal, FXACCOUNTS_PERMISSION,
                           Ci.nsIPermissionManager.PROMPT_ACTION);

  FxAccountsUIGlue._activeSession = {
    email: "user@domain.org",
    verified: true,
    sessionToken: "1234"
  };

  FxAccountsManager.getAssertion("audience", promptPermissionsPrincipal).then(
    result => {
      do_check_false(FxAccountsUIGlue._signInFlowCalled);
      do_check_true(FxAccountsUIGlue._refreshAuthCalled);
      do_check_eq(result, "assertion");

      let permission = permMan.testPermissionFromPrincipal(
        promptPermissionsPrincipal,
        FXACCOUNTS_PERMISSION
      );
      do_check_eq(permission, Ci.nsIPermissionManager.ALLOW_ACTION);
      FxAccountsManager._fxAccounts._reset();
      FxAccountsUIGlue._reset();
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_getAssertion_permission_prompt_action_refreshing) {
  do_print("= getAssertion PROMPT_ACTION permission already refreshing =");

  let promptPermissionsPrincipal = makePrincipal('app://dummy-prompt-2', 30);
  let permMan = Cc["@mozilla.org/permissionmanager;1"]
                  .getService(Ci.nsIPermissionManager);
  permMan.addFromPrincipal(promptPermissionsPrincipal, FXACCOUNTS_PERMISSION,
                           Ci.nsIPermissionManager.PROMPT_ACTION);

  FxAccountsUIGlue._activeSession = {
    email: "user@domain.org",
    verified: true,
    sessionToken: "1234"
  };

  FxAccountsManager._refreshing = true;

  FxAccountsManager.getAssertion("audience", promptPermissionsPrincipal).then(
    result => {
      do_check_false(FxAccountsUIGlue._signInFlowCalled);
      do_check_false(FxAccountsUIGlue._refreshAuthCalled);
      do_check_null(result);

      let permission = permMan.testPermissionFromPrincipal(
        promptPermissionsPrincipal,
        FXACCOUNTS_PERMISSION
      );
      do_check_eq(permission, Ci.nsIPermissionManager.PROMPT_ACTION);

      FxAccountsManager._refreshing = false;

      FxAccountsManager._fxAccounts._reset();
      FxAccountsUIGlue._reset();
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_getAssertion_server_state_change) {
  FxAccountsManager._fxAccounts._signedInUser.verified = true;
  FxAccountsManager._activeSession.verified = true;
  passwordResetOnServer = true;
  FxAccountsManager.getAssertion("audience", principal).then(
    (result) => {
      // For password reset, the UIGlue mock simulates sucessful
      // refreshAuth which supplies new password, not signin/signup.
      do_check_true(FxAccountsUIGlue._refreshAuthCalled);
      do_check_false(FxAccountsUIGlue._signInFlowCalled)
      do_check_eq(result, "assertion");
      FxAccountsUIGlue._refreshAuthCalled = false;
    }
  ).then(
    () => {
      deletedOnServer = true;
      FxAccountsManager.getAssertion("audience", principal).then(
        (result) => {
          // For account deletion, the UIGlue's signin/signup is called.
          do_check_true(FxAccountsUIGlue._signInFlowCalled)
          do_check_false(FxAccountsUIGlue._refreshAuthCalled);
          do_check_eq(result, "assertion");
          deletedOnServer = false;
          passwordResetOnServer = false;
          FxAccountsUIGlue._reset()
          run_next_test();
        }
      );
    }
  );
});

add_test(function(test_getAssertion_refreshAuth_NaN) {
  do_print("= getAssertion refreshAuth NaN=");
  let gracePeriod = "NaN";
  FxAccountsManager.getAssertion("audience", principal, {
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
  FxAccountsManager.getAssertion("audience", principal, {
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
      do_check_eq(result.email, FxAccountsManager._user.email);
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
      do_check_eq(result.email, FxAccountsManager._user.email);
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
  FxAccountsManager.getAccount();
  do_execute_soon(function() {
    do_check_true(FakeFxAccountsClient._recoveryEmailStatusCalled);
    FxAccountsManager.getAccount().then(
      result => {
        do_check_true(result.verified);
        do_check_eq(result.email, FxAccountsManager._user.email);
        FakeFxAccountsClient._reset();
        run_next_test();
    });
  });
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

add_test(function(test_signUp_no_email) {
  do_print("= signUp, no email=");
  FxAccountsManager.signUp().then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_INVALID_EMAIL);
      run_next_test();
    }
  );
});

add_test(function(test_signIn_no_email) {
  do_print("= signIn, no email=");
  FxAccountsManager.signIn().then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_INVALID_EMAIL);
      run_next_test();
    }
  );
});

add_test(function(test_signUp_no_password) {
  do_print("= signUp, no email=");
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

add_test(function(test_signIn_no_email) {
  do_print("= signIn, no email=");
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
      do_check_eq(result.user.email, "user@domain.org");
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
      do_check_eq(error.details.user.email, "user@domain.org");
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
      do_check_eq(error.details.user.email, "user@domain.org");
      do_check_false(error.details.user.verified);
      run_next_test();
    }
  );
});

add_test(function(test_resendVerificationEmail_error_handling) {
  do_print("= resendVerificationEmail smoke test =");
  let user = FxAccountsManager._fxAccounts._signedInUser;
  FxAccountsManager._fxAccounts._signedInUser.verified = false;
  FxAccountsManager.resendVerificationEmail().then(
    (success) => {
      do_check_true(success);
    },
    (error) => {
      do_throw("Unexpected failure");
    }
  );
  // Here we verify that when FxAccounts.resendVerificationEmail
  // throws an error, we gracefully handle it in the reject() channel.
  FxAccountsManager._fxAccounts._signedInUser = null;
  FxAccountsManager.resendVerificationEmail().then(
    (success) => {
      do_throw("Unexpected success");
    },
    (error) => {
      do_check_eq(error.error, ERROR_SERVER_ERROR);
    }
  );
  FxAccountsManager._fxAccounts._signedInUser = user;
  run_next_test();
});

add_test(function(test_verificationStatus_unverified_session_unverified_user) {
  do_print("= verificationStatus unverified session and user =");
  FakeFxAccountsClient._verified = false;
  FxAccountsManager.verificationStatus();
  do_execute_soon(function() {
    let user = FxAccountsManager._user;
    do_check_false(user.verified);
    do_check_true(FakeFxAccountsClient._recoveryEmailStatusCalled);
    do_check_false(FxAccountsManager._fxAccounts._setSignedInUserCalled);
    run_next_test();
  });
});

add_test(function(test_verificationStatus_unverified_session_verified_user) {
  do_print("= verificationStatus unverified session, verified user =");
  FakeFxAccountsClient._verified = true;
  FxAccountsManager.verificationStatus();
  do_execute_soon(function() {
    let user = FxAccountsManager._user;
    do_check_true(user.verified);
    do_check_true(FakeFxAccountsClient._recoveryEmailStatusCalled);
    do_check_true(FxAccountsManager._fxAccounts._setSignedInUserCalled);
    run_next_test();
  });
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

add_test(function(test_queryAccount_no_email) {
  do_print("= queryAccount, no email =");
  FxAccountsManager.queryAccount().then(
    () => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_INVALID_EMAIL);
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

add_test(function(test_getKeys_sync_disabled) {
  do_print("= getKeys sync disabled =");
  Services.prefs.setBoolPref("services.sync.enabled", false);
  FxAccountsManager.getKeys().then(
    result => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error, ERROR_SYNC_DISABLED);
      Services.prefs.clearUserPref("services.sync.enabled");
      run_next_test();
    }
  );
});

add_test(function(test_getKeys_no_session) {
  do_print("= getKeys no session =");
  Services.prefs.setBoolPref("services.sync.enabled", true);
  FxAccountsManager._fxAccounts._signedInUser = null;
  FxAccountsManager._activeSession = null;
  FxAccountsManager.getKeys().then(
    result => {
      do_check_null(result);
      FxAccountsManager._fxAccounts._reset();
      Services.prefs.clearUserPref("services.sync.enabled");
      run_next_test();
    },
    error => {
      do_throw("Unexpected error: " + error);
    }
  );
});

add_test(function(test_getKeys_unverified_account) {
  do_print("= getKeys unverified =");
  Services.prefs.setBoolPref("services.sync.enabled", true);
  FakeFxAccountsClient._verified = false;
  FxAccountsManager.signIn("user@domain.org", "password").then(result => {
    do_check_false(result.verified);
    return FxAccountsManager.getKeys();
  }).then(result => {
      do_throw("Unexpected success");
    },
    error => {
      do_check_eq(error.error, ERROR_UNVERIFIED_ACCOUNT);
      FxAccountsManager._fxAccounts._reset();
      Services.prefs.clearUserPref("services.sync.enabled");
      FxAccountsManager.signOut().then(run_next_test)
    }
  );
});

add_test(function(test_getKeys_success) {
  do_print("= getKeys success =");
  Services.prefs.setBoolPref("services.sync.enabled", true);
  FakeFxAccountsClient._verified = true;
  FxAccountsManager.signIn("user@domain.org", "password").then(result => {
    return FxAccountsManager.getKeys();
  }).then(result => {
      do_check_eq(result, FxAccountsManager._fxAccounts._keys);
      FxAccountsManager._fxAccounts._reset();
      Services.prefs.clearUserPref("services.sync.enabled");
      run_next_test();
    },
    error => {
      do_throw("Unexpected error " + error);
    }
  );
});
