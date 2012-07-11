/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["IdentityService"];

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/identity/LogUtils.jsm");
Cu.import("resource://gre/modules/identity/IdentityStore.jsm");
Cu.import("resource://gre/modules/identity/RelyingParty.jsm");
Cu.import("resource://gre/modules/identity/IdentityProvider.jsm");

XPCOMUtils.defineLazyModuleGetter(this,
                                  "jwcrypto",
                                  "resource://gre/modules/identity/jwcrypto.jsm");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["core"].concat(aMessageArgs));
}
function reportError(...aMessageArgs) {
  Logger.reportError.apply(Logger, ["core"].concat(aMessageArgs));
}

function IDService() {
  Services.obs.addObserver(this, "quit-application-granted", false);
  Services.obs.addObserver(this, "identity-auth-complete", false);

  this._store = IdentityStore;
  this.RP = RelyingParty;
  this.IDP = IdentityProvider;
}

IDService.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "quit-application-granted":
        Services.obs.removeObserver(this, "quit-application-granted");
        this.shutdown();
        break;
      case "identity-auth-complete":
        if (!aSubject || !aSubject.wrappedJSObject)
          break;
        let subject = aSubject.wrappedJSObject;
        log("Auth complete:", aSubject.wrappedJSObject);
        // We have authenticated in order to provision an identity.
        // So try again.
        this.selectIdentity(subject.rpId, subject.identity);
        break;
    }
  },

  reset: function reset() {
    // Explicitly call reset() on our RP and IDP classes.
    // This is here to make testing easier.  When the
    // quit-application-granted signal is emitted, reset() will be
    // called here, on RP, on IDP, and on the store.  So you don't
    // need to use this :)
    this._store.reset();
    this.RP.reset();
    this.IDP.reset();
  },

  shutdown: function shutdown() {
    log("shutdown");
    Services.obs.removeObserver(this, "identity-auth-complete");
    Services.obs.removeObserver(this, "quit-application-granted");
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
   * The UX wants to add a new identity
   * often followed by selectIdentity()
   *
   * @param aIdentity
   *        (string) the email chosen for login
   */
  addIdentity: function addIdentity(aIdentity) {
    if (this._store.fetchIdentity(aIdentity) === null) {
      this._store.addIdentity(aIdentity, null, null);
    }
  },

  /**
   * The UX comes back and calls selectIdentity once the user has picked
   * an identity.
   *
   * @param aRPId
   *        (integer) the id of the doc object obtained in .watch() and
   *                  passed to the UX component.
   *
   * @param aIdentity
   *        (string) the email chosen for login
   */
  selectIdentity: function selectIdentity(aRPId, aIdentity) {
    log("selectIdentity: RP id:", aRPId, "identity:", aIdentity);

    // Get the RP that was stored when watch() was invoked.
    let rp = this.RP._rpFlows[aRPId];
    if (!rp) {
      reportError("selectIdentity", "Invalid RP id: ", aRPId);
      return;
    }

    // It's possible that we are in the process of provisioning an
    // identity.
    let provId = rp.provId;

    let rpLoginOptions = {
      loggedInEmail: aIdentity,
      origin: rp.origin
    };
    log("selectIdentity: provId:", provId, "origin:", rp.origin);

    // Once we have a cert, and once the user is authenticated with the
    // IdP, we can generate an assertion and deliver it to the doc.
    let self = this;
    this.RP._generateAssertion(rp.origin, aIdentity, function hadReadyAssertion(err, assertion) {
      if (!err && assertion) {
        self.RP._doLogin(rp, rpLoginOptions, assertion);
        return;

      }
      // Need to provision an identity first.  Begin by discovering
      // the user's IdP.
      self._discoverIdentityProvider(aIdentity, function gotIDP(err, idpParams) {
        if (err) {
          rp.doError(err);
          return;
        }

        // The idpParams tell us where to go to provision and authenticate
        // the identity.
        self.IDP._provisionIdentity(aIdentity, idpParams, provId, function gotID(err, aProvId) {

          // Provision identity may have created a new provision flow
          // for us.  To make it easier to relate provision flows with
          // RP callers, we cross index the two here.
          rp.provId = aProvId;
          self.IDP._provisionFlows[aProvId].rpId = aRPId;

          // At this point, we already have a cert.  If the user is also
          // already authenticated with the IdP, then we can try again
          // to generate an assertion and login.
          if (err) {
            // We are not authenticated.  If we have already tried to
            // authenticate and failed, then this is a "hard fail" and
            // we give up.  Otherwise we try to authenticate with the
            // IdP.

            if (self.IDP._provisionFlows[aProvId].didAuthentication) {
              self.IDP._cleanUpProvisionFlow(aProvId);
              self.RP._cleanUpProvisionFlow(aRPId, aProvId);
              log("ERROR: selectIdentity: authentication hard fail");
              rp.doError("Authentication fail.");
              return;
            }
            // Try to authenticate with the IdP.  Note that we do
            // not clean up the provision flow here.  We will continue
            // to use it.
            self.IDP._doAuthentication(aProvId, idpParams);
            return;
          }

          // Provisioning flows end when a certificate has been registered.
          // Thus IdentityProvider's registerCertificate() cleans up the
          // current provisioning flow.  We only do this here on error.
          self.RP._generateAssertion(rp.origin, aIdentity, function gotAssertion(err, assertion) {
            if (err) {
              rp.doError(err);
              return;
            }
            self.RP._doLogin(rp, rpLoginOptions, assertion);
            self.RP._cleanUpProvisionFlow(aRPId, aProvId);
            return;
          });
        });
      });
    });
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
                .getService(Ci.nsIXMLHttpRequest);

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

let IdentityService = new IDService();
