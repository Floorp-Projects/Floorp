/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
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

XPCOMUtils.defineLazyModuleGetter(this, "objectCopy",
                                  "resource://gre/modules/identity/IdentityUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "makeMessageObject",
                                  "resource://gre/modules/identity/IdentityUtils.jsm");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["minimal core"].concat(aMessageArgs));
}
function reportError(...aMessageArgs) {
  Logger.reportError.apply(Logger, ["core"].concat(aMessageArgs));
}

function IDService() {
  Services.obs.addObserver(this, "quit-application-granted", false);

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
        this.shutdown();
        break;
    }
  },

  shutdown: function() {
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
    Services.obs.notifyObservers({wrappedJSObject: options}, "identity-controller-watch", null);
  },

  /*
   * The RP has gone away; remove handles to the hidden iframe.
   * It's probable that the frame will already have been cleaned up.
   */
  unwatch: function unwatch(aRpId, aTargetMM) {
    let rp = this._rpFlows[aRpId];
    if (!rp) {
      return;
    }

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
    if (!rp) {
      reportError("request() called before watch()");
      return;
    }

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
    if (!rp) {
      reportError("logout() called before watch()");
      return;
    }

    let options = makeMessageObject(rp);
    Services.obs.notifyObservers({wrappedJSObject: options}, "identity-controller-logout", null);
  },

  childProcessShutdown: function childProcessShutdown(messageManager) {
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
      log("WARNING: doLogin found no rp to go with callerId " + aRpCallerId);
      return;
    }

    rp.doLogin(aAssertion, aInternalParams);
  },

  doLogout: function doLogout(aRpCallerId) {
    let rp = this._rpFlows[aRpCallerId];
    if (!rp) {
      log("WARNING: doLogout found no rp to go with callerId " + aRpCallerId);
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
      log("WARNING: doReady found no rp to go with callerId " + aRpCallerId);
      return;
    }

    rp.doReady();
  },

  doCancel: function doCancel(aRpCallerId) {
    let rp = this._rpFlows[aRpCallerId];
    if (!rp) {
      log("WARNING: doCancel found no rp to go with callerId " + aRpCallerId);
      return;
    }

    rp.doCancel();
  }
};

this.IdentityService = new IDService();
