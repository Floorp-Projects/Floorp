/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Temporary abstraction layer for common Fx Accounts operations.
 * For now, we will be using this module only from B2G but in the end we might
 * want this to be merged with FxAccounts.jsm and let other products also use
 * it.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["FxAccountsManager"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");

XPCOMUtils.defineLazyServiceGetter(this, "permissionManager",
                                   "@mozilla.org/permissionmanager;1",
                                   "nsIPermissionManager");

this.FxAccountsManager = {

  init: function() {
    Services.obs.addObserver(this, ONLOGOUT_NOTIFICATION, false);
    Services.obs.addObserver(this, ON_FXA_UPDATE_NOTIFICATION, false);
  },

  observe: function(aSubject, aTopic, aData) {
    // Both topics indicate our cache is invalid
    this._activeSession = null;

    if (aData == ONVERIFIED_NOTIFICATION) {
      log.debug("FxAccountsManager: cache cleared, broadcasting: " + aData);
      Services.obs.notifyObservers(null, aData, null);
    }
  },

  // We don't really need to save fxAccounts instance but this way we allow
  // to mock FxAccounts from tests.
  _fxAccounts: fxAccounts,

  // We keep the session details here so consumers don't need to deal with
  // session tokens and are only required to handle the email.
  _activeSession: null,

  // Are we refreshing our authentication? If so, allow attempts to sign in
  // while we are already signed in.
  _refreshing: false,

  // We only expose the email and the verified status so far.
  get _user() {
    if (!this._activeSession || !this._activeSession.email) {
      return null;
    }

    return {
      email: this._activeSession.email,
      verified: this._activeSession.verified
    }
  },

  _error: function(aError, aDetails) {
    log.error(aError);
    let reason = {
      error: aError
    };
    if (aDetails) {
      reason.details = aDetails;
    }
    return Promise.reject(reason);
  },

  _getError: function(aServerResponse) {
    if (!aServerResponse || !aServerResponse.error || !aServerResponse.error.errno) {
      return;
    }
    let error = SERVER_ERRNO_TO_ERROR[aServerResponse.error.errno];
    return error;
  },

  _serverError: function(aServerResponse) {
    let error = this._getError({ error: aServerResponse });
    return this._error(error ? error : ERROR_SERVER_ERROR, aServerResponse);
  },

  // As with _fxAccounts, we don't really need this method, but this way we
  // allow tests to mock FxAccountsClient.  By default, we want to return the
  // client used by the fxAccounts object because deep down they should have
  // access to the same hawk request object which will enable them to share
  // local clock skeq data.
  _getFxAccountsClient: function() {
    return this._fxAccounts.getAccountsClient();
  },

  _signInSignUp: function(aMethod, aEmail, aPassword) {
    if (Services.io.offline) {
      return this._error(ERROR_OFFLINE);
    }

    if (!aEmail) {
      return this._error(ERROR_INVALID_EMAIL);
    }

    if (!aPassword) {
      return this._error(ERROR_INVALID_PASSWORD);
    }

    // Check that there is no signed in account first.
    if ((!this._refreshing) && this._activeSession) {
      return this._error(ERROR_ALREADY_SIGNED_IN_USER, {
        user: this._user
      });
    }

    let client = this._getFxAccountsClient();
    return this._fxAccounts.getSignedInUser().then(
      user => {
        if ((!this._refreshing) && user) {
          return this._error(ERROR_ALREADY_SIGNED_IN_USER, {
            user: this._user
          });
        }
        let syncEnabled = false;
        try {
          syncEnabled = Services.prefs.getBoolPref("services.sync.enabled");
        } catch(e) {
          dump(e + "\n");
        }
        // XXX Refetch FxA credentials if services.sync.enabled preference
        //     changes. Bug 1183103
        return client[aMethod](aEmail, aPassword, syncEnabled);
      }
    ).then(
      user => {
        let error = this._getError(user);
        if (!user || !user.uid || !user.sessionToken || error) {
          return this._error(error ? error : ERROR_INTERNAL_INVALID_USER, {
            user: user
          });
        }

        // If the user object includes an email field, it may differ in
        // capitalization from what we sent down.  This is the server's
        // canonical capitalization and should be used instead.
        user.email = user.email || aEmail;

        // If we're using server-side sign to refreshAuthentication
        // we don't need to update local state; also because of two
        // interacting glitches we need to bypass an event emission.
        // See https://bugzilla.mozilla.org/show_bug.cgi?id=1031580
        if (this._refreshing) {
          return Promise.resolve({user: this._user});
        }

        return this._fxAccounts.setSignedInUser(user).then(
          () => {
            this._activeSession = user;
            log.debug("User signed in: " + JSON.stringify(this._user) +
                      " - Account created " + (aMethod == "signUp"));
            return Promise.resolve({
              accountCreated: aMethod === "signUp",
              user: this._user
            });
          }
        );
      },
      reason => { return this._serverError(reason); }
    );
  },

  /**
   * Determine whether the incoming error means that the current account
   * has new server-side state via deletion or password change, and if so,
   * spawn the appropriate UI (sign in or refresh); otherwise re-reject.
   *
   * As of May 2014, the only HTTP call triggered by this._getAssertion()
   * is to /certificate/sign via:
   *   FxAccounts.getAssertion()
   *     FxAccountsInternal.getCertificateSigned()
   *       FxAccountsClient.signCertificate()
   * See the latter method for possible (error code, errno) pairs.
   */
  _handleGetAssertionError: function(reason, aAudience, aPrincipal) {
    log.debug("FxAccountsManager._handleGetAssertionError()");
    let errno = (reason ? reason.errno : NaN) || NaN;
    // If the previously valid email/password pair is no longer valid ...
    if (errno == ERRNO_INVALID_AUTH_TOKEN) {
      return this._fxAccounts.accountStatus().then(
        (exists) => {
          // ... if the email still maps to an account, the password
          // must have changed, so ask the user to enter the new one ...
          if (exists) {
            return this.getAccount().then(
              (user) => {
                return this._refreshAuthentication(aAudience, user.email,
                                                   aPrincipal,
                                                   true /* logoutOnFailure */);
              }
            );
          }
          // ... otherwise, the account was deleted, so ask for Sign In/Up
          return this._localSignOut().then(
            () => {
              return this._uiRequest(UI_REQUEST_SIGN_IN_FLOW, aAudience,
                                     aPrincipal);
            },
            (reason) => {
              // reject primary problem, not signout failure
              log.error("Signing out in response to server error threw: " +
                        reason);
              return this._error(reason);
            }
          );
        }
      );
    }
    return Promise.reject(reason);
  },

  _getAssertion: function(aAudience, aPrincipal) {
    return this._fxAccounts.getAssertion(aAudience).then(
      (result) => {
        if (aPrincipal) {
          this._addPermission(aPrincipal);
        }
        return result;
      },
      (reason) => {
        return this._handleGetAssertionError(reason, aAudience, aPrincipal);
      }
    );
  },

  /**
   * "Refresh authentication" means:
   *   Interactively demonstrate knowledge of the FxA password
   *   for the currently logged-in account.
   * There are two very different scenarios:
   *   1) The password has changed on the server. Failure should log
   *      the current account OUT.
   *   2) The person typing can't prove knowledge of the password used
   *      to log in. Failure should do nothing.
   */
  _refreshAuthentication: function(aAudience, aEmail, aPrincipal,
                                   logoutOnFailure=false) {
    this._refreshing = true;
    return this._uiRequest(UI_REQUEST_REFRESH_AUTH,
                           aAudience, aPrincipal, aEmail).then(
      (assertion) => {
        this._refreshing = false;
        return assertion;
      },
      (reason) => {
        this._refreshing = false;
        if (logoutOnFailure) {
          return this._signOut().then(
            () => {
              return this._error(reason);
            }
          );
        }
        return this._error(reason);
      }
    );
  },

  _localSignOut: function() {
    return this._fxAccounts.signOut(true);
  },

  _signOut: function() {
    if (!this._activeSession) {
      return Promise.resolve();
    }

    // We clear the local session cache as soon as we get the onlogout
    // notification triggered within FxAccounts.signOut, so we save the
    // session token value to be able to remove the remote server session
    // in case that we have network connection.
    let sessionToken = this._activeSession.sessionToken;

    return this._localSignOut().then(
      () => {
        // At this point the local session should already be removed.

        // The client can create new sessions up to the limit (100?).
        // Orphaned tokens on the server will eventually be garbage collected.
        if (Services.io.offline) {
          return Promise.resolve();
        }
        // Otherwise, we try to remove the remote session.
        let client = this._getFxAccountsClient();
        return client.signOut(sessionToken).then(
          result => {
            let error = this._getError(result);
            if (error) {
              return this._error(error, result);
            }
            log.debug("Signed out");
            return Promise.resolve();
          },
          reason => {
            return this._serverError(reason);
          }
        );
      }
    );
  },

  _uiRequest: function(aRequest, aAudience, aPrincipal, aParams) {
    if (Services.io.offline) {
      return this._error(ERROR_OFFLINE);
    }
    let ui = Cc["@mozilla.org/fxaccounts/fxaccounts-ui-glue;1"]
               .createInstance(Ci.nsIFxAccountsUIGlue);
    if (!ui[aRequest]) {
      return this._error(ERROR_UI_REQUEST);
    }

    if (!aParams || !Array.isArray(aParams)) {
      aParams = [aParams];
    }

    return ui[aRequest].apply(this, aParams).then(
      result => {
        // Even if we get a successful result from the UI, the account will
        // most likely be unverified, so we cannot get an assertion.
        if (result && result.verified) {
          return this._getAssertion(aAudience, aPrincipal);
        }

        return this._error(ERROR_UNVERIFIED_ACCOUNT, {
          user: result
        });
      },
      error => {
        return this._error(ERROR_UI_ERROR, error);
      }
    );
  },

  _addPermission: function(aPrincipal) {
    // This will fail from tests cause we are running them in the child
    // process until we have chrome tests in b2g. Bug 797164.
    try {
      permissionManager.addFromPrincipal(aPrincipal, FXACCOUNTS_PERMISSION,
                                         Ci.nsIPermissionManager.ALLOW_ACTION);
    } catch (e) {
      log.warn("Could not add permission " + e);
    }
  },

  // -- API --

  signIn: function(aEmail, aPassword) {
    return this._signInSignUp("signIn", aEmail, aPassword);
  },

  signUp: function(aEmail, aPassword) {
    return this._signInSignUp("signUp", aEmail, aPassword);
  },

  signOut: function() {
    if (!this._activeSession) {
      // If there is no cached active session, we try to get it from the
      // account storage.
      return this.getAccount().then(
        result => {
          if (!result) {
            return Promise.resolve();
          }
          return this._signOut();
        }
      );
    }
    return this._signOut();
  },

  resendVerificationEmail: function() {
    return this._fxAccounts.resendVerificationEmail().then(
      (result) => {
        return result;
      },
      (error) => {
        return this._error(ERROR_SERVER_ERROR, error);
      }
    );
  },

  getAccount: function() {
    // We check first if we have session details cached.
    if (this._activeSession) {
      // If our cache says that the account is not yet verified,
      // we kick off verification before returning what we have.
      if (!this._activeSession.verified) {
        this.verificationStatus(this._activeSession);
      }
      log.debug("Account " + JSON.stringify(this._user));
      return Promise.resolve(this._user);
    }

    // If no cached information, we try to get it from the persistent storage.
    return this._fxAccounts.getSignedInUser().then(
      user => {
        if (!user || !user.email) {
          log.debug("No signed in account");
          return Promise.resolve(null);
        }

        this._activeSession = user;
        // If we get a stored information of a not yet verified account,
        // we kick off verification before returning what we have.
        if (!user.verified) {
          this.verificationStatus(user);
        }

        log.debug("Account " + JSON.stringify(this._user));
        return Promise.resolve(this._user);
      }
    );
  },

  queryAccount: function(aEmail) {
    log.debug("queryAccount " + aEmail);
    if (Services.io.offline) {
      return this._error(ERROR_OFFLINE);
    }

    let deferred = Promise.defer();

    if (!aEmail) {
      return this._error(ERROR_INVALID_EMAIL);
    }

    let client = this._getFxAccountsClient();
    return client.accountExists(aEmail).then(
      result => {
        log.debug("Account " + result ? "" : "does not" + " exists");
        let error = this._getError(result);
        if (error) {
          return this._error(error, result);
        }

        return Promise.resolve({
          registered: result
        });
      },
      reason => { this._serverError(reason); }
    );
  },

  verificationStatus: function() {
    log.debug("verificationStatus");
    if (!this._activeSession || !this._activeSession.sessionToken) {
      this._error(ERROR_NO_TOKEN_SESSION);
    }

    // There is no way to unverify an already verified account, so we just
    // return the account details of a verified account
    if (this._activeSession.verified) {
      log.debug("Account already verified");
      return;
    }

    if (Services.io.offline) {
      log.warn("Offline; skipping verification.");
      return;
    }

    let client = this._getFxAccountsClient();
    client.recoveryEmailStatus(this._activeSession.sessionToken).then(
      data => {
        let error = this._getError(data);
        if (error) {
          this._error(error, data);
        }
        // If the verification status has changed, update state.
        if (this._activeSession.verified != data.verified) {
          this._activeSession.verified = data.verified;
          this._fxAccounts.setSignedInUser(this._activeSession);
        }
        log.debug(JSON.stringify(this._user));
      },
      reason => { this._serverError(reason); }
    );
  },

  /*
   * Try to get an assertion for the given audience. Here we implement
   * the heart of the response to navigator.mozId.request() on device.
   * (We can also be called via the IAC API, but it's request() that
   * makes this method complex.) The state machine looks like this,
   * ignoring simple errors:
   *   If no one is signed in, and we aren't suppressing the UI:
   *     trigger the sign in flow.
   *   else if we were asked to refresh and the grace period is up:
   *     trigger the refresh flow.
   *   else:
   *      request user permission to share an assertion if we don't have it
   *      already and ask the core code for an assertion, which might itself
   *      trigger either the sign in or refresh flows (if our account
   *      changed on the server).
   *
   * aOptions can include:
   *   refreshAuthentication  - (bool) Force re-auth.
   *   silent                 - (bool) Prevent any UI interaction.
   *                            I.e., try to get an automatic assertion.
   */
  getAssertion: function(aAudience, aPrincipal, aOptions) {
    if (!aAudience) {
      return this._error(ERROR_INVALID_AUDIENCE);
    }

    let principal = aPrincipal;
    log.debug("FxAccountsManager.getAssertion() aPrincipal: ",
              principal.origin, principal.appId, principal.isInBrowserElement);

    return this.getAccount().then(
      user => {
        if (user) {
          // Three have-user cases to consider. First: are we unverified?
          if (!user.verified) {
            return this._error(ERROR_UNVERIFIED_ACCOUNT, {
              user: user
            });
          }
          // Second case: do we need to refresh?
          if (aOptions &&
              (typeof(aOptions.refreshAuthentication) != "undefined")) {
            let gracePeriod = aOptions.refreshAuthentication;
            if (typeof(gracePeriod) !== "number" || isNaN(gracePeriod)) {
              return this._error(ERROR_INVALID_REFRESH_AUTH_VALUE);
            }
            // Forcing refreshAuth to silent is a contradiction in terms,
            // though it might succeed silently if we didn't reject here.
            if (aOptions.silent) {
              return this._error(ERROR_NO_SILENT_REFRESH_AUTH);
            }
            let secondsSinceAuth = (Date.now() / 1000) -
                                   this._activeSession.authAt;
            if (secondsSinceAuth > gracePeriod) {
              return this._refreshAuthentication(aAudience, user.email,
                                                 principal,
                                                 false /* logoutOnFailure */);
            }
          }
          // Third case: we are all set *locally*. Probably we just return
          // the assertion, but the attempt might lead to the server saying
          // we are deleted or have a new password, which will trigger a flow.
          // Also we need to check if we have permission to get the assertion,
          // otherwise we need to show the forceAuth UI to let the user know
          // that the RP with no fxa permissions is trying to obtain an
          // assertion. Once the user authenticates herself in the forceAuth UI
          // the permission will be remembered by default.
          let permission = permissionManager.testPermissionFromPrincipal(
            principal,
            FXACCOUNTS_PERMISSION
          );
          if (permission == Ci.nsIPermissionManager.PROMPT_ACTION &&
              !this._refreshing) {
            return this._refreshAuthentication(aAudience, user.email,
                                               principal,
                                               false /* logoutOnFailure */);
          } else if (permission == Ci.nsIPermissionManager.DENY_ACTION &&
                     !this._refreshing) {
            return this._error(ERROR_PERMISSION_DENIED);
          } else if (this._refreshing) {
            // If we are blocked asking for a password we should not continue
            // the getAssertion process.
            return Promise.resolve(null);
          }
          return this._getAssertion(aAudience, principal);
        }
        log.debug("No signed in user");
        if (aOptions && aOptions.silent) {
          return Promise.resolve(null);
        }
        return this._uiRequest(UI_REQUEST_SIGN_IN_FLOW, aAudience, principal);
      }
    );
  },

  getKeys: function() {
    let syncEnabled = false;
    try {
      syncEnabled = Services.prefs.getBoolPref("services.sync.enabled");
    } catch(e) {
      dump("Sync is disabled, so you won't get the keys. " + e + "\n");
    }

    if (!syncEnabled) {
      return Promise.reject(ERROR_SYNC_DISABLED);
    }

    return this.getAccount().then(
      user => {
        if (!user) {
          log.debug("No signed in user");
          return Promise.resolve(null);
        }

        if (!user.verified) {
          return this._error(ERROR_UNVERIFIED_ACCOUNT, {
            user: user
          });
        }

        return this._fxAccounts.getKeys();
      }
    );
  }
};

FxAccountsManager.init();
