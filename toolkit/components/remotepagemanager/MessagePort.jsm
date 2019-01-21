/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MessagePort", "MessageListener"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "AsyncPrefs",
  "resource://gre/modules/AsyncPrefs.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

/*
 * Used for all kinds of permissions checks which requires explicit
 * whitelisting of specific permissions granted through RPM.
 *
 * Please note that prefs that one wants to update need to be
 * whitelisted within AsyncPrefs.jsm.
 */
let RPMAccessManager = {
  accessMap: {
    "about:privatebrowsing": {
      // "sendAsyncMessage": handled within AboutPrivateBrowsingHandler.jsm
      // "setBoolPref": handled within AsyncPrefs.jsm and uses the pref
      //                ["privacy.trackingprotection.pbmode.enabled"],
      "getBoolPref": ["privacy.trackingprotection.pbmode.enabled"],
      "getFormatURLPref": ["privacy.trackingprotection.introURL",
                           "app.support.baseURL"],
      "isWindowPrivate": ["yes"],
    },
  },

  checkAllowAccess(aPrincipal, aFeature, aValue) {
    // if there is no content principal; deny access
    if (!aPrincipal || !aPrincipal.URI) {
      return false;
    }
    let uri = aPrincipal.URI.asciiSpec;

    // check if there is an entry for that requestying URI in the accessMap;
    // if not, deny access.
    let accessMapForURI = this.accessMap[uri];
    if (!accessMapForURI) {
      Cu.reportError("RPMAccessManager does not allow access to Feature: " + aFeature + " for: " + uri);
      return false;
    }

    // check if the feature is allowed to be accessed for that URI;
    // if not, deny access.
    let accessMapForFeature = accessMapForURI[aFeature];
    if (!accessMapForFeature) {
      Cu.reportError("RPMAccessManager does not allow access to Feature: " + aFeature + " for: " + uri);
      return false;
    }

    // if the actual value is in the whitelist for that feature;
    // allow access
    if (accessMapForFeature.includes(aValue)) {
      return true;
    }

    // otherwise deny access
    Cu.reportError("RPMAccessManager does not allow access to Feature: " + aFeature + " for: " + uri);
    return false;
  },
};

function MessageListener() {
  this.listeners = new Map();
}

MessageListener.prototype = {
  keys() {
    return this.listeners.keys();
  },

  has(name) {
    return this.listeners.has(name);
  },

  callListeners(message) {
    let listeners = this.listeners.get(message.name);
    if (!listeners) {
      return;
    }

    for (let listener of listeners.values()) {
      try {
        listener(message);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  },

  addMessageListener(name, callback) {
    if (!this.listeners.has(name))
      this.listeners.set(name, new Set([callback]));
    else
      this.listeners.get(name).add(callback);
  },

  removeMessageListener(name, callback) {
    if (!this.listeners.has(name))
      return;

    this.listeners.get(name).delete(callback);
  },
};

/*
 * A message port sits on each side of the process boundary for every remote
 * page. Each has a port ID that is unique to the message manager it talks
 * through.
 *
 * We roughly implement the same contract as nsIMessageSender and
 * nsIMessageListenerManager
 */
function MessagePort(messageManager, portID) {
  this.messageManager = messageManager;
  this.portID = portID;
  this.destroyed = false;
  this.listener = new MessageListener();

  this.message = this.message.bind(this);
  this.messageManager.addMessageListener("RemotePage:Message", this.message);
}

MessagePort.prototype = {
  messageManager: null,
  portID: null,
  destroyed: null,
  listener: null,
  _browser: null,
  remotePort: null,

  // Called when the message manager used to connect to the other process has
  // changed, i.e. when a tab is detached.
  swapMessageManager(messageManager) {
    this.messageManager.removeMessageListener("RemotePage:Message", this.message);

    this.messageManager = messageManager;

    this.messageManager.addMessageListener("RemotePage:Message", this.message);
  },

  /* Adds a listener for messages. Many callbacks can be registered for the
   * same message if necessary. An attempt to register the same callback for the
   * same message twice will be ignored. When called the callback is passed an
   * object with these properties:
   *   target: This message port
   *   name:   The message name
   *   data:   Any data sent with the message
   */
  addMessageListener(name, callback) {
    if (this.destroyed) {
      throw new Error("Message port has been destroyed");
    }

    this.listener.addMessageListener(name, callback);
  },

  /*
   * Removes a listener for messages.
   */
  removeMessageListener(name, callback) {
    if (this.destroyed) {
      throw new Error("Message port has been destroyed");
    }

    this.listener.removeMessageListener(name, callback);
  },

  // Sends a message asynchronously to the other process
  sendAsyncMessage(name, data = null) {
    if (this.destroyed) {
      throw new Error("Message port has been destroyed");
    }

    this.messageManager.sendAsyncMessage("RemotePage:Message", {
      portID: this.portID,
      name,
      data,
    });
  },

  // Called to destroy this port
  destroy() {
    try {
      // This can fail in the child process if the tab has already been closed
      this.messageManager.removeMessageListener("RemotePage:Message", this.message);
    } catch (e) { }
    this.messageManager = null;
    this.destroyed = true;
    this.portID = null;
    this.listener = null;
  },

  getBoolPref(aPref) {
    let principal = this.window.document.nodePrincipal;
    if (!RPMAccessManager.checkAllowAccess(principal, "getBoolPref", aPref)) {
      throw new Error("RPMAccessManager does not allow access to getBoolPref");
    }
    return Services.prefs.getBoolPref(aPref);
  },

  setBoolPref(aPref, aVal) {
    return new this.window.Promise(function(resolve) {
      AsyncPrefs.set(aPref, aVal).then(function() {
        resolve();
      });
    });
  },

  getFormatURLPref(aFormatURL) {
    let principal = this.window.document.nodePrincipal;
    if (!RPMAccessManager.checkAllowAccess(principal, "getFormatURLPref", aFormatURL)) {
      throw new Error("RPMAccessManager does not allow access to getFormatURLPref");
    }
    return Services.urlFormatter.formatURLPref(aFormatURL);
  },

  isWindowPrivate() {
    let principal = this.window.document.nodePrincipal;
    if (!RPMAccessManager.checkAllowAccess(principal, "isWindowPrivate", "yes")) {
      throw new Error("RPMAccessManager does not allow access to isWindowPrivate");
    }
    return PrivateBrowsingUtils.isContentWindowPrivate(this.window);
  },
};
