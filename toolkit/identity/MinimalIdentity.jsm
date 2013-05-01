/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This alternate implementation of IdentityService provides just the
 * channels for navigator.id, leaving the certificate storage to a
 * server-provided app.
 *
 * On b2g, the messages identity-controller-watch, -request, and
 * -logout, are observed by the component SignInToWebsite.jsm.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["IdentityService"];

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/identity/LogUtils.jsm");
Cu.import("resource://gre/modules/identity/IdentityUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this,
                                  "jwcrypto",
                                  "resource://gre/modules/identity/jwcrypto.jsm");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["minimal core"].concat(aMessageArgs));
}
function reportError(...aMessageArgs) {
  Logger.reportError.apply(Logger, ["core"].concat(aMessageArgs));
}

function makeMessageObject(aRpCaller) {
  let options = {};

  options.id = aRpCaller.id;
  options.origin = aRpCaller.origin;

  // loggedInUser can be undefined, null, or a string
  options.loggedInUser = aRpCaller.loggedInUser;

  // Special flag for internal calls
  options._internal = aRpCaller._internal;

  Object.keys(aRpCaller).forEach(function(option) {
    // Duplicate the callerobject, scrubbing out functions and other
    // internal variables (like _mm, the message manager object)
    if (!Object.hasOwnProperty(this, option)
        && option[0] !== '_'
        && typeof aRpCaller[option] !== 'function') {
      options[option] = aRpCaller[option];
    }
  });

  // check validity of message structure
  if ((typeof options.id === 'undefined') ||
      (typeof options.origin === 'undefined')) {
    let err = "id and origin required in relying-party message: " + JSON.stringify(options);
    reportError(err);
    throw new Error(err);
  }

  return options;
}

function IDService() {
  Services.obs.addObserver(this, "quit-application-granted", false);
  // Services.obs.addObserver(this, "identity-auth-complete", false);

  // simplify, it's one object
  this.RP = this;
  this.IDP = this;

  // keep track of flows
  this._rpFlows = {};
  this._authFlows = {};
  this._provFlows = {};
}

IDService.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "quit-application-granted":
        Services.obs.removeObserver(this, "quit-application-granted");
        // Services.obs.removeObserver(this, "identity-auth-complete");
        break;
    }
  },

  /**
   * Parse an email into username and domain if it is valid, else return null
   */
  parseEmail: function parseEmail(email) {
    var match = email.match(/^([^@]+)@([^@^/]+.[a-z]+)$/);
    if (match) {
      return {
        username: match[1],
        domain: match[2]
      };
    }
    return null;
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
    // store the caller structure and notify the UI observers
    this._rpFlows[aRpCaller.id] = aRpCaller;

    log("flows:", Object.keys(this._rpFlows).join(', '));

    let options = makeMessageObject(aRpCaller);
    log("sending identity-controller-watch:", options);
    Services.obs.notifyObservers({wrappedJSObject: options},"identity-controller-watch", null);
  },

  /*
   * The RP has gone away; remove handles to the hidden iframe.
   * It's probable that the frame will already have been cleaned up.
   */
  unwatch: function unwatch(aRpId, aTargetMM) {
    let rp = this._rpFlows[aRpId];
    let options = makeMessageObject({
      id: aRpId,
      origin: rp.origin,
      messageManager: aTargetMM
    });
    log("sending identity-controller-unwatch for id", options.id, options.origin);
    Services.obs.notifyObservers({wrappedJSObject: options}, "identity-controller-unwatch", null);

    // Stop sending messages to this window
    delete this._rpFlows[aRpId];
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
    let rp = this._rpFlows[aRPId];

    // Notify UX to display identity picker.
    // Pass the doc id to UX so it can pass it back to us later.
    let options = makeMessageObject(rp);
    objectCopy(aOptions, options);
    Services.obs.notifyObservers({wrappedJSObject: options}, "identity-controller-request", null);
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
    let rp = this._rpFlows[aRpCallerId];

    let options = makeMessageObject(rp);
    Services.obs.notifyObservers({wrappedJSObject: options}, "identity-controller-logout", null);
  },

  childProcessShutdown: function childProcessShutdown(messageManager) {
    let options = makeMessageObject({messageManager: messageManager, id: null, origin: null});
    Services.obs.notifyObservers({wrappedJSObject: options}, "identity-child-process-shutdown", null);
    Object.keys(this._rpFlows).forEach(function(key) {
      if (this._rpFlows[key]._mm === messageManager) {
        log("child process shutdown for rp", key, "- deleting flow");
        delete this._rpFlows[key];
      }
    }, this);
  },

  /*
   * once the UI-and-display-logic components have received
   * notifications, they call back with direct invocation of the
   * following functions (doLogin, doLogout, or doReady)
   */

  doLogin: function doLogin(aRpCallerId, aAssertion, aInternalParams) {
    let rp = this._rpFlows[aRpCallerId];
    if (!rp) {
      dump("WARNING: doLogin found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doLogin(aAssertion, aInternalParams);
  },

  doLogout: function doLogout(aRpCallerId) {
    let rp = this._rpFlows[aRpCallerId];
    if (!rp) {
      dump("WARNING: doLogout found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    // Logout from every site with the same origin
    let origin = rp.origin;
    Object.keys(this._rpFlows).forEach(function(key) {
      let rp = this._rpFlows[key];
      if (rp.origin === origin) {
        rp.doLogout();
      }
    }.bind(this));
  },

  doReady: function doReady(aRpCallerId) {
    let rp = this._rpFlows[aRpCallerId];
    if (!rp) {
      dump("WARNING: doReady found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doReady();
  },

  doCancel: function doCancel(aRpCallerId) {
    let rp = this._rpFlows[aRpCallerId];
    if (!rp) {
      dump("WARNING: doCancel found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doCancel();
  },


  /*
   * XXX Bug 804229: Implement Identity Provider Functions
   *
   * Stubs for Identity Provider functions follow
   */

  /**
   * the provisioning iframe sandbox has called navigator.id.beginProvisioning()
   *
   * @param aCaller
   *        (object)  the iframe sandbox caller with all callbacks and
   *                  other information.  Callbacks include:
   *                  - doBeginProvisioningCallback(id, duration_s)
   *                  - doGenKeyPairCallback(pk)
   */
  beginProvisioning: function beginProvisioning(aCaller) {
  },

  /**
   * the provisioning iframe sandbox has called
   * navigator.id.raiseProvisioningFailure()
   *
   * @param aProvId
   *        (int)  the identifier of the provisioning flow tied to that sandbox
   * @param aReason
   */
  raiseProvisioningFailure: function raiseProvisioningFailure(aProvId, aReason) {
    reportError("Provisioning failure", aReason);
  },

  /**
   * When navigator.id.genKeyPair is called from provisioning iframe sandbox.
   * Generates a keypair for the current user being provisioned.
   *
   * @param aProvId
   *        (int)  the identifier of the provisioning caller tied to that sandbox
   *
   * It is an error to call genKeypair without receiving the callback for
   * the beginProvisioning() call first.
   */
  genKeyPair: function genKeyPair(aProvId) {
  },

  /**
   * When navigator.id.registerCertificate is called from provisioning iframe
   * sandbox.
   *
   * Sets the certificate for the user for which a certificate was requested
   * via a preceding call to beginProvisioning (and genKeypair).
   *
   * @param aProvId
   *        (integer) the identifier of the provisioning caller tied to that
   *                  sandbox
   *
   * @param aCert
   *        (String)  A JWT representing the signed certificate for the user
   *                  being provisioned, provided by the IdP.
   */
  registerCertificate: function registerCertificate(aProvId, aCert) {
  },

  /**
   * The authentication frame has called navigator.id.beginAuthentication
   *
   * IMPORTANT: the aCaller is *always* non-null, even if this is called from
   * a regular content page. We have to make sure, on every DOM call, that
   * aCaller is an expected authentication-flow identifier. If not, we throw
   * an error or something.
   *
   * @param aCaller
   *        (object)  the authentication caller
   *
   */
  beginAuthentication: function beginAuthentication(aCaller) {
  },

  /**
   * The auth frame has called navigator.id.completeAuthentication
   *
   * @param aAuthId
   *        (int)  the identifier of the authentication caller tied to that sandbox
   *
   */
  completeAuthentication: function completeAuthentication(aAuthId) {
  },

  /**
   * The auth frame has called navigator.id.cancelAuthentication
   *
   * @param aAuthId
   *        (int)  the identifier of the authentication caller
   *
   */
  cancelAuthentication: function cancelAuthentication(aAuthId) {
  },

  // methods for chrome and add-ons

  /**
   * Discover the IdP for an identity
   *
   * @param aIdentity
   *        (string) the email we're logging in with
   *
   * @param aCallback
   *        (function) callback to invoke on completion
   *                   with first-positional parameter the error.
   */
  _discoverIdentityProvider: function _discoverIdentityProvider(aIdentity, aCallback) {
    // XXX bug 767610 - validate email address call
    // When that is available, we can remove this custom parser
    var parsedEmail = this.parseEmail(aIdentity);
    if (parsedEmail === null) {
      return aCallback("Could not parse email: " + aIdentity);
    }
    log("_discoverIdentityProvider: identity:", aIdentity, "domain:", parsedEmail.domain);

    this._fetchWellKnownFile(parsedEmail.domain, function fetchedWellKnown(err, idpParams) {
      // idpParams includes the pk, authorization url, and
      // provisioning url.

      // XXX bug 769861 follow any authority delegations
      // if no well-known at any point in the delegation
      // fall back to browserid.org as IdP
      return aCallback(err, idpParams);
    });
  },

  /**
   * Fetch the well-known file from the domain.
   *
   * @param aDomain
   *
   * @param aScheme
   *        (string) (optional) Protocol to use.  Default is https.
   *                 This is necessary because we are unable to test
   *                 https.
   *
   * @param aCallback
   *
   */
  _fetchWellKnownFile: function _fetchWellKnownFile(aDomain, aCallback, aScheme='https') {
    // XXX bug 769854 make tests https and remove aScheme option
    let url = aScheme + '://' + aDomain + "/.well-known/browserid";
    log("_fetchWellKnownFile:", url);

    // this appears to be a more successful way to get at xmlhttprequest (which supposedly will close with a window
    let req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);

    // XXX bug 769865 gracefully handle being off-line
    // XXX bug 769866 decide on how to handle redirects
    req.open("GET", url, true);
    req.responseType = "json";
    req.mozBackgroundRequest = true;
    req.onload = function _fetchWellKnownFile_onload() {
      if (req.status < 200 || req.status >= 400) {
        log("_fetchWellKnownFile", url, ": server returned status:", req.status);
        return aCallback("Error");
      }
      try {
        let idpParams = req.response;

        // Verify that the IdP returned a valid configuration
        if (! (idpParams.provisioning &&
            idpParams.authentication &&
            idpParams['public-key'])) {
          let errStr= "Invalid well-known file from: " + aDomain;
          log("_fetchWellKnownFile:", errStr);
          return aCallback(errStr);
        }

        let callbackObj = {
          domain: aDomain,
          idpParams: idpParams,
        };
        log("_fetchWellKnownFile result: ", callbackObj);
        // Yay.  Valid IdP configuration for the domain.
        return aCallback(null, callbackObj);

      } catch (err) {
        reportError("_fetchWellKnownFile", "Bad configuration from", aDomain, err);
        return aCallback(err.toString());
      }
    };
    req.onerror = function _fetchWellKnownFile_onerror() {
      log("_fetchWellKnownFile", "ERROR:", req.status, req.statusText);
      log("ERROR: _fetchWellKnownFile:", err);
      return aCallback("Error");
    };
    req.send(null);
  },

};

this.IdentityService = new IDService();
