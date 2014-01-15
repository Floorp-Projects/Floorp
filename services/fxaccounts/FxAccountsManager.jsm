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

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsClient",
  "resource://gre/modules/FxAccountsClient.jsm");

this.FxAccountsManager = {

  init: function() {
    Services.obs.addObserver(this, ONLOGOUT_NOTIFICATION, false);
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic !== ONLOGOUT_NOTIFICATION) {
      return;
    }

    // Remove the cached session if we get a logout notification.
    this._activeSession = null;
  },

  // We don't really need to save fxAccounts instance but this way we allow
  // to mock FxAccounts from tests.
  _fxAccounts: fxAccounts,

  // We keep the session details here so consumers don't need to deal with
  // session tokens and are only required to handle the email.
  _activeSession: null,

  // We only expose the email and the verified status so far.
  get _user() {
    if (!this._activeSession || !this._activeSession.email) {
      return null;
    }

    return {
      accountId: this._activeSession.email,
      verified: this._activeSession.verified
    }
  },

  _getError: function(aServerResponse) {
    if (!aServerResponse || !aServerResponse.error || !aServerResponse.error.errno) {
      return;
    }
    let error = SERVER_ERRNO_TO_ERROR[aServerResponse.error.errno];
    log.error(error);
    return error;
  },

  _serverError: function(aServerResponse) {
    let error = this._getError({ error: aServerResponse });
    return Promise.reject({
      error: error ? error : ERROR_SERVER_ERROR,
      details: aServerResponse
    });
  },

  // As we do with _fxAccounts, we don't really need this factory, but this way
  // we allow tests to mock FxAccountsClient.
  _createFxAccountsClient: function() {
    return new FxAccountsClient();
  },

  _signInSignUp: function(aMethod, aAccountId, aPassword) {
    if (Services.io.offline) {
      log.error(ERROR_OFFLINE);
      return Promise.reject({
        error: ERROR_OFFLINE
      });
    }

    if (!aAccountId) {
      log.error(ERROR_INVALID_ACCOUNTID);
      return Promise.reject({
        error: ERROR_INVALID_ACCOUNTID
      });
    }

    if (!aPassword) {
      log.error(ERROR_INVALID_PASSWORD);
      return Promise.reject({
        error: ERROR_INVALID_PASSWORD
      });
    }

    // Check that there is no signed in account first.
    if (this._activeSession) {
      log.error(ERROR_ALREADY_SIGNED_IN_USER);
      return Promise.reject({
        error: ERROR_ALREADY_SIGNED_IN_USER,
        details: {
          user: this._user
        }
      });
    }

    let client = this._createFxAccountsClient();
    return this._fxAccounts.getSignedInUser().then(
      user => {
        if (user) {
          log.error(ERROR_ALREADY_SIGNED_IN_USER);
          return Promise.reject({
            error: ERROR_ALREADY_SIGNED_IN_USER,
            details: {
              user: user
            }
          });
        }
        return client[aMethod](aAccountId, aPassword);
      }
    ).then(
      user => {
        let error = this._getError(user);
        if (!user || !user.uid || !user.sessionToken || error) {
          log.error(error ? error : ERROR_INTERNAL_INVALID_USER);
          return Promise.reject({
            error: error ? error : ERROR_INTERNAL_INVALID_USER,
            details: {
              user: user
            }
          });
        }

        // Save the credentials of the signed in user.
        user.email = aAccountId;
        return this._fxAccounts.setSignedInUser(user, false).then(
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

  _getAssertion: function(aAudience) {
    return this._fxAccounts.getAssertion(aAudience);
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

    return this._fxAccounts.signOut(sessionToken).then(
      () => {
        // At this point the local session should already be removed.

        // The client can create new sessions up to the limit (100?).
        // Orphaned tokens on the server will eventually be garbage collected.
        if (Services.io.offline) {
          return Promise.resolve();
        }
        // Otherwise, we try to remove the remote session.
        let client = this._createFxAccountsClient();
        return client.signOut(sessionToken).then(
          result => {
            let error = this._getError(result);
            if (error) {
              return Promise.reject({
                error: error,
                details: result
              });
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

  // -- API --

  signIn: function(aAccountId, aPassword) {
    return this._signInSignUp("signIn", aAccountId, aPassword);
  },

  signUp: function(aAccountId, aPassword) {
    return this._signInSignUp("signUp", aAccountId, aPassword);
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

  getAccount: function() {
    // We check first if we have session details cached.
    if (this._activeSession) {
      // If our cache says that the account is not yet verified, we check that
      // this information is correct, and update the cached data if not.
      if (this._activeSession && !this._activeSession.verified &&
          !Services.io.offline) {
        return this.verificationStatus(this._activeSession);
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
        // we check this information with the server, update the stored
        // data if needed and finally return the account details.
        if (!user.verified && !Services.io.offline) {
          log.debug("Unverified account");
          return this.verificationStatus(user);
        }

        log.debug("Account " + JSON.stringify(this._user));
        return Promise.resolve(this._user);
      }
    );
  },

  queryAccount: function(aAccountId) {
    log.debug("queryAccount " + aAccountId);
    if (Services.io.offline) {
      log.error(ERROR_OFFLINE);
      return Promise.reject({
        error: ERROR_OFFLINE
      });
    }

    let deferred = Promise.defer();

    if (!aAccountId) {
      log.error(ERROR_INVALID_ACCOUNTID);
      return Promise.reject({
        error: ERROR_INVALID_ACCOUNTID
      });
    }

    let client = this._createFxAccountsClient();
    return client.accountExists(aAccountId).then(
      result => {
        log.debug("Account " + result ? "" : "does not" + " exists");
        let error = this._getError(result);
        if (error) {
          return Promise.reject({
            error: error,
            details: result
          });
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
      log.error(ERROR_NO_TOKEN_SESSION);
      return Promise.reject({
        error: ERROR_NO_TOKEN_SESSION
      });
    }

    // There is no way to unverify an already verified account, so we just
    // return the account details of a verified account
    if (this._activeSession.verified) {
      log.debug("Account already verified");
      return Promise.resolve(this._user);
    }

    if (Services.io.offline) {
      log.error(ERROR_OFFLINE);
      return Promise.reject({
        error: ERROR_OFFLINE
      });
    }

    let client = this._createFxAccountsClient();
    return client.recoveryEmailStatus(this._activeSession.sessionToken).then(
      data => {
        let error = this._getError(data);
        if (error) {
          return Promise.reject({
            error: error,
            details: data
          });
        }

        // If the verification status is different from the one that we have
        // stored, we update it and return the session data. If not, we simply
        // return the session data.
        if (this._activeSession.verified != data.verified) {
          this._activeSession.verified = data.verified;
          return this._fxAccounts.setSignedInUser(this._activeSession).then(
            () => {
              log.debug(JSON.stringify(this._user));
              return Promise.resolve(this._user);
            }
          );
        }
        log.debug(JSON.stringify(this._user));
        return Promise.resolve(this._user);
      },
      reason => { return this._serverError(reason); }
    );
  },

  getAssertion: function(aAudience) {
    log.debug("getAssertion " + aAudience);
    if (!aAudience) {
      log.error(ERROR_INVALID_AUDIENCE);
      return Promise.reject({
        error: ERROR_INVALID_AUDIENCE
      });
    }

    if (Services.io.offline) {
      log.error(ERROR_OFFLINE);
      return Promise.reject({
        error: ERROR_OFFLINE
      });
    }

    return this.getAccount().then(
      user => {
        if (user) {
          // We cannot get assertions for unverified accounts.
          if (user.verified) {
            return this._getAssertion(aAudience);
          }

          log.error(ERROR_UNVERIFIED_ACCOUNT);
          return Promise.reject({
            error: ERROR_UNVERIFIED_ACCOUNT,
            details: {
              user: user
            }
          });
        }

        log.debug("No signed in user");
        // If there is no currently signed in user, we trigger the signIn UI
        // flow.
        let ui = Cc["@mozilla.org/fxaccounts/fxaccounts-ui-glue;1"]
                   .createInstance(Ci.nsIFxAccountsUIGlue);
        return ui.signInFlow().then(
          result => {
            // Even if we get a successful result from the UI, the account will
            // most likely be unverified, so we cannot get an assertion.
            if (result && result.verified) {
              return this._getAssertion(aAudience);
            }

            log.error(ERROR_UNVERIFIED_ACCOUNT);
            return Promise.reject({
              error: ERROR_UNVERIFIED_ACCOUNT,
              details: {
                user: result
              }
            });
          },
          error => {
            log.error(ERROR_UI_ERROR + " " + error);
            return Promise.reject({
              error: ERROR_UI_ERROR,
              details: error
            });
          }
        );
      }
    );
  }
};

FxAccountsManager.init();
