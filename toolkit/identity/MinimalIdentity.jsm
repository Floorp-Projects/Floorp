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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/identity/IdentityUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "logger", function() {
  Cu.import('resource://gre/modules/identity/LogUtils.jsm');
  return getLogger("Identity", "toolkit.identity.debug");
});

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
    logger.error(err);
    throw new Error(err);
  }

  return options;
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

  observe: function IDService_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "quit-application-granted":
        Services.obs.removeObserver(this, "quit-application-granted");
        break;
    }
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
  watch: function IDService_watch(aRpCaller) {
    // store the caller structure and notify the UI observers
    this._rpFlows[aRpCaller.id] = aRpCaller;

    let options = makeMessageObject(aRpCaller);
    logger.log("sending identity-controller-watch:", options);
    Services.obs.notifyObservers({wrappedJSObject: options},"identity-controller-watch", null);
  },

  /*
   * The RP has gone away; remove handles to the hidden iframe.
   * It's probable that the frame will already have been cleaned up.
   */
  unwatch: function IDService_unwatch(aRpId, aTargetMM) {
    let rp = this._rpFlows[aRpId];
    let options = makeMessageObject({
      id: aRpId,
      origin: rp.origin,
      messageManager: aTargetMM
    });
    logger.log("sending identity-controller-unwatch for id", options.id, options.origin);
    Services.obs.notifyObservers({wrappedJSObject: options}, "identity-controller-unwatch", null);
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
  request: function IDService_request(aRPId, aOptions) {
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
  logout: function IDService_logout(aRpCallerId) {
    let rp = this._rpFlows[aRpCallerId];

    let options = makeMessageObject(rp);
    Services.obs.notifyObservers({wrappedJSObject: options}, "identity-controller-logout", null);
  },

  childProcessShutdown: function IDService_childProcessShutdown(messageManager) {
    let options = makeMessageObject({messageManager: messageManager, id: null, origin: null});
    Services.obs.notifyObservers({wrappedJSObject: options}, "identity-child-process-shutdown", null);
    Object.keys(this._rpFlows).forEach(function(key) {
      if (this._rpFlows[key]._mm === messageManager) {
        logger.log("child process shutdown for rp", key, "- deleting flow");
        delete this._rpFlows[key];
      }
    }, this);
  },

  /*
   * once the UI-and-display-logic components have received
   * notifications, they call back with direct invocation of the
   * following functions (doLogin, doLogout, or doReady)
   */

  doLogin: function IDService_doLogin(aRpCallerId, aAssertion, aInternalParams) {
    let rp = this._rpFlows[aRpCallerId];
    if (!rp) {
      dump("WARNING: doLogin found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doLogin(aAssertion, aInternalParams);
  },

  doLogout: function IDService_doLogout(aRpCallerId) {
    let rp = this._rpFlows[aRpCallerId];
    if (!rp) {
      dump("WARNING: doLogout found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doLogout();
  },

  doReady: function IDService_doReady(aRpCallerId) {
    let rp = this._rpFlows[aRpCallerId];
    if (!rp) {
      dump("WARNING: doReady found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doReady();
  },

  doCancel: function IDService_doCancel(aRpCallerId) {
    let rp = this._rpFlows[aRpCallerId];
    if (!rp) {
      dump("WARNING: doCancel found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doCancel();
  }
};

this.IdentityService = new IDService();
