/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/identity/LogUtils.jsm");
Cu.import("resource://gre/modules/identity/IdentityStore.jsm");

this.EXPORTED_SYMBOLS = ["RelyingParty"];

XPCOMUtils.defineLazyModuleGetter(this, "objectCopy",
                                  "resource://gre/modules/identity/IdentityUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this,
                                  "jwcrypto",
                                  "resource://gre/modules/identity/jwcrypto.jsm");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["RP"].concat(aMessageArgs));
}
function reportError(...aMessageArgs) {
  Logger.reportError.apply(Logger, ["RP"].concat(aMessageArgs));
}

function IdentityRelyingParty() {
  // The store is a singleton shared among Identity, RelyingParty, and
  // IdentityProvider.  The Identity module takes care of resetting
  // state in the _store on shutdown.
  this._store = IdentityStore;

  this.reset();
}

IdentityRelyingParty.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "quit-application-granted":
        Services.obs.removeObserver(this, "quit-application-granted");
        this.shutdown();
        break;

    }
  },

  reset: function RP_reset() {
    // Forget all documents that call in.  (These are sometimes
    // referred to as callers.)
    this._rpFlows = {};
  },

  shutdown: function RP_shutdown() {
    this.reset();
    Services.obs.removeObserver(this, "quit-application-granted");
  },

  /**
   * Register a listener for a given windowID as a result of a call to
   * navigator.id.watch().
   *
   * @param aCaller
   *        (Object)  an object that represents the caller document, and
   *                  is expected to have properties:
   *                  - id (unique, e.g. uuid)
   *                  - loggedInUser (string or null)
   *                  - origin (string)
   *
   *                  and a bunch of callbacks
   *                  - doReady()
   *                  - doLogin()
   *                  - doLogout()
   *                  - doError()
   *                  - doCancel()
   *
   */
  watch: function watch(aRpCaller) {
    this._rpFlows[aRpCaller.id] = aRpCaller;
    let origin = aRpCaller.origin;
    let state = this._store.getLoginState(origin) || { isLoggedIn: false, email: null };

    log("watch: rpId:", aRpCaller.id,
        "origin:", origin,
        "loggedInUser:", aRpCaller.loggedInUser,
        "loggedIn:", state.isLoggedIn,
        "email:", state.email);

    // If the user is already logged in, then there are three cases
    // to deal with:
    //
    //   1. the email is valid and unchanged:  'ready'
    //   2. the email is null:                 'login'; 'ready'
    //   3. the email has changed:             'login'; 'ready'
    if (state.isLoggedIn) {
      if (state.email && aRpCaller.loggedInUser === state.email) {
        this._notifyLoginStateChanged(aRpCaller.id, state.email);
        return aRpCaller.doReady();

      } else if (aRpCaller.loggedInUser === null) {
        // Generate assertion for existing login
        let options = {loggedInUser: state.email, origin: origin};
        return this._doLogin(aRpCaller, options);

      } else {
        // A loggedInUser different from state.email has been specified.
        // Change login identity.

        let options = {loggedInUser: state.email, origin: origin};
        return this._doLogin(aRpCaller, options);
      }

    // If the user is not logged in, there are two cases:
    //
    //   1. a logged in email was provided: 'ready'; 'logout'
    //   2. not logged in, no email given:  'ready';

    } else {
      if (aRpCaller.loggedInUser) {
        return this._doLogout(aRpCaller, {origin: origin});

      } else {
        return aRpCaller.doReady();
      }
    }
  },

  /**
   * A utility for watch() to set state and notify the dom
   * on login
   *
   * Note that this calls _getAssertion
   */
  _doLogin: function _doLogin(aRpCaller, aOptions, aAssertion) {
    log("_doLogin: rpId:", aRpCaller.id, "origin:", aOptions.origin);

    let loginWithAssertion = function loginWithAssertion(assertion) {
      this._store.setLoginState(aOptions.origin, true, aOptions.loggedInUser);
      this._notifyLoginStateChanged(aRpCaller.id, aOptions.loggedInUser);
      aRpCaller.doLogin(assertion);
      aRpCaller.doReady();
    }.bind(this);

    if (aAssertion) {
      loginWithAssertion(aAssertion);
    } else {
      this._getAssertion(aOptions, function gotAssertion(err, assertion) {
        if (err) {
          reportError("_doLogin:", "Failed to get assertion on login attempt:", err);
          this._doLogout(aRpCaller);
        } else {
          loginWithAssertion(assertion);
        }
      }.bind(this));
    }
  },

  /**
   * A utility for watch() to set state and notify the dom
   * on logout.
   */
  _doLogout: function _doLogout(aRpCaller, aOptions) {
    log("_doLogout: rpId:", aRpCaller.id, "origin:", aOptions.origin);

    let state = this._store.getLoginState(aOptions.origin) || {};

    state.isLoggedIn = false;
    this._notifyLoginStateChanged(aRpCaller.id, null);

    aRpCaller.doLogout();
    aRpCaller.doReady();
  },

  /**
   * For use with login or logout, emit 'identity-login-state-changed'
   *
   * The notification will send the rp caller id in the properties,
   * and the email of the user in the message.
   *
   * @param aRpCallerId
   *        (integer) The id of the RP caller
   *
   * @param aIdentity
   *        (string) The email of the user whose login state has changed
   */
  _notifyLoginStateChanged: function _notifyLoginStateChanged(aRpCallerId, aIdentity) {
    log("_notifyLoginStateChanged: rpId:", aRpCallerId, "identity:", aIdentity);

    let options = {rpId: aRpCallerId};
    Services.obs.notifyObservers({wrappedJSObject: options},
                                 "identity-login-state-changed",
                                 aIdentity);
  },

  /**
   * Initiate a login with user interaction as a result of a call to
   * navigator.id.request().
   *
   * @param aRPId
   *        (integer)  the id of the doc object obtained in .watch()
   *
   * @param aOptions
   *        (Object)  options including privacyPolicy, termsOfService
   */
  request: function request(aRPId, aOptions) {
    log("request: rpId:", aRPId);
    let rp = this._rpFlows[aRPId];

    // Notify UX to display identity picker.
    // Pass the doc id to UX so it can pass it back to us later.
    let options = {rpId: aRPId, origin: rp.origin};
    objectCopy(aOptions, options);

    // Append URLs after resolving
    let baseURI = Services.io.newURI(rp.origin, null, null);
    for (let optionName of ["privacyPolicy", "termsOfService"]) {
      if (aOptions[optionName]) {
        options[optionName] = baseURI.resolve(aOptions[optionName]);
      }
    }

    Services.obs.notifyObservers({wrappedJSObject: options}, "identity-request", null);
  },

  /**
   * Invoked when a user wishes to logout of a site (for instance, when clicking
   * on an in-content logout button).
   *
   * @param aRpCallerId
   *        (integer)  the id of the doc object obtained in .watch()
   *
   */
  logout: function logout(aRpCallerId) {
    log("logout: RP caller id:", aRpCallerId);
    let rp = this._rpFlows[aRpCallerId];
    if (rp && rp.origin) {
      let origin = rp.origin;
      log("logout: origin:", origin);
      this._doLogout(rp, {origin: origin});
    } else {
      log("logout: no RP found with id:", aRpCallerId);
    }
    // We don't delete this._rpFlows[aRpCallerId], because
    // the user might log back in again.
  },

  getDefaultEmailForOrigin: function getDefaultEmailForOrigin(aOrigin) {
    let identities = this.getIdentitiesForSite(aOrigin);
    let result = identities.lastUsed || null;
    log("getDefaultEmailForOrigin:", aOrigin, "->", result);
    return result;
  },

  /**
   * Return the list of identities a user may want to use to login to aOrigin.
   */
  getIdentitiesForSite: function getIdentitiesForSite(aOrigin) {
    let rv = { result: [] };
    for (let id in this._store.getIdentities()) {
      rv.result.push(id);
    }
    let loginState = this._store.getLoginState(aOrigin);
    if (loginState && loginState.email)
      rv.lastUsed = loginState.email;
    return rv;
  },

  /**
   * Obtain a BrowserID assertion with the specified characteristics.
   *
   * @param aCallback
   *        (Function) Callback to be called with (err, assertion) where 'err'
   *        can be an Error or NULL, and 'assertion' can be NULL or a valid
   *        BrowserID assertion. If no callback is provided, an exception is
   *        thrown.
   *
   * @param aOptions
   *        (Object) An object that may contain the following properties:
   *
   *          "audience"      : The audience for which the assertion is to be
   *                            issued. If this property is not set an exception
   *                            will be thrown.
   *
   *        Any properties not listed above will be ignored.
   */
  _getAssertion: function _getAssertion(aOptions, aCallback) {
    let audience = aOptions.origin;
    let email = aOptions.loggedInUser || this.getDefaultEmailForOrigin(audience);
    log("_getAssertion: audience:", audience, "email:", email);
    if (!audience) {
      throw "audience required for _getAssertion";
    }

    // We might not have any identity info for this email
    if (!this._store.fetchIdentity(email)) {
      this._store.addIdentity(email, null, null);
    }

    let cert = this._store.fetchIdentity(email)['cert'];
    if (cert) {
      this._generateAssertion(audience, email, function generatedAssertion(err, assertion) {
        if (err) {
          log("ERROR: _getAssertion:", err);
        }
        log("_getAssertion: generated assertion:", assertion);
        return aCallback(err, assertion);
      });
    }
  },

  /**
   * Generate an assertion, including provisioning via IdP if necessary,
   * but no user interaction, so if provisioning fails, aCallback is invoked
   * with an error.
   *
   * @param aAudience
   *        (string) web origin
   *
   * @param aIdentity
   *        (string) the email we're logging in with
   *
   * @param aCallback
   *        (function) callback to invoke on completion
   *                   with first-positional parameter the error.
   */
  _generateAssertion: function _generateAssertion(aAudience, aIdentity, aCallback) {
    log("_generateAssertion: audience:", aAudience, "identity:", aIdentity);

    let id = this._store.fetchIdentity(aIdentity);
    if (! (id && id.cert)) {
      let errStr = "Cannot generate an assertion without a certificate";
      log("ERROR: _generateAssertion:", errStr);
      aCallback(errStr);
      return;
    }

    let kp = id.keyPair;

    if (!kp) {
      let errStr = "Cannot generate an assertion without a keypair";
      log("ERROR: _generateAssertion:", errStr);
      aCallback(errStr);
      return;
    }

    jwcrypto.generateAssertion(id.cert, kp, aAudience, aCallback);
  },

  /**
   * Clean up references to the provisioning flow for the specified RP.
   */
  _cleanUpProvisionFlow: function RP_cleanUpProvisionFlow(aRPId, aProvId) {
    let rp = this._rpFlows[aRPId];
    if (rp) {
      delete rp['provId'];
    } else {
      log("Error: Couldn't delete provision flow ", aProvId, " for RP ", aRPId);
    }
  },

};

this.RelyingParty = new IdentityRelyingParty();
