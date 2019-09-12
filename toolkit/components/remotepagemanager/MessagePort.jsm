/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MessagePort", "MessageListener"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "AsyncPrefs",
  "resource://gre/modules/AsyncPrefs.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);

/*
 * Used for all kinds of permissions checks which requires explicit
 * whitelisting of specific permissions granted through RPM.
 *
 * Please note that prefs that one wants to update need to be
 * whitelisted within AsyncPrefs.jsm.
 */
let RPMAccessManager = {
  accessMap: {
    "about:certerror": {
      getFormatURLPref: ["app.support.baseURL"],
      getBoolPref: [
        "security.certerrors.mitm.priming.enabled",
        "security.enterprise_roots.auto-enabled",
        "security.certerror.hideAddException",
        "security.ssl.errorReporting.automatic",
        "security.ssl.errorReporting.enabled",
      ],
      getIntPref: [
        "services.settings.clock_skew_seconds",
        "services.settings.last_update_seconds",
      ],
      getAppBuildID: ["yes"],
      recordTelemetryEvent: ["yes"],
    },
    "about:privatebrowsing": {
      // "sendAsyncMessage": handled within AboutPrivateBrowsingHandler.jsm
      getFormatURLPref: ["app.support.baseURL"],
      isWindowPrivate: ["yes"],
    },
    "about:protections": {
      getBoolPref: [
        "browser.contentblocking.report.lockwise.enabled",
        "browser.contentblocking.report.monitor.enabled",
        "privacy.socialtracking.block_cookies.enabled",
        "browser.contentblocking.report.proxy.enabled",
      ],
      getStringPref: [
        "browser.contentblocking.category",
        "browser.contentblocking.report.lockwise.url",
        "browser.contentblocking.report.monitor.url",
        "browser.contentblocking.report.monitor.sign_in_url",
        "browser.contentblocking.report.manage_devices.url",
        "browser.contentblocking.report.proxy_extension.url",
      ],
      getFormatURLPref: [
        "browser.contentblocking.report.monitor.how_it_works.url",
        "browser.contentblocking.report.lockwise.how_it_works.url",
        "browser.contentblocking.report.social.url",
        "browser.contentblocking.report.cookie.url",
        "browser.contentblocking.report.tracker.url",
        "browser.contentblocking.report.fingerprinter.url",
        "browser.contentblocking.report.cryptominer.url",
      ],
      recordTelemetryEvent: ["yes"],
    },
    "about:newinstall": {
      getUpdateChannel: ["yes"],
      getFxAccountsEndpoint: ["yes"],
    },
  },

  checkAllowAccess(aPrincipal, aFeature, aValue) {
    // if there is no content principal; deny access
    if (!aPrincipal || !aPrincipal.URI) {
      return false;
    }

    let uri = aPrincipal.URI.asciiSpec;
    if (uri.startsWith("about:certerror")) {
      uri = "about:certerror";
    }

    // check if there is an entry for that requestying URI in the accessMap;
    // if not, deny access.
    let accessMapForURI = this.accessMap[uri];
    if (!accessMapForURI) {
      Cu.reportError(
        "RPMAccessManager does not allow access to Feature: " +
          aFeature +
          " for: " +
          uri
      );
      return false;
    }

    // check if the feature is allowed to be accessed for that URI;
    // if not, deny access.
    let accessMapForFeature = accessMapForURI[aFeature];
    if (!accessMapForFeature) {
      Cu.reportError(
        "RPMAccessManager does not allow access to Feature: " +
          aFeature +
          " for: " +
          uri
      );
      return false;
    }

    // if the actual value is in the whitelist for that feature;
    // allow access
    if (accessMapForFeature.includes(aValue)) {
      return true;
    }

    // otherwise deny access
    Cu.reportError(
      "RPMAccessManager does not allow access to Feature: " +
        aFeature +
        " for: " +
        uri
    );
    return false;
  },
};

class MessageListener {
  constructor() {
    this.listeners = new Map();
  }

  keys() {
    return this.listeners.keys();
  }

  has(name) {
    return this.listeners.has(name);
  }

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
  }

  addMessageListener(name, callback) {
    if (!this.listeners.has(name)) {
      this.listeners.set(name, new Set([callback]));
    } else {
      this.listeners.get(name).add(callback);
    }
  }

  removeMessageListener(name, callback) {
    if (!this.listeners.has(name)) {
      return;
    }

    this.listeners.get(name).delete(callback);
  }
}

/*
 * A message port sits on each side of the process boundary for every remote
 * page. Each has a port ID that is unique to the message manager it talks
 * through.
 *
 * We roughly implement the same contract as nsIMessageSender and
 * nsIMessageListenerManager
 */
class MessagePort {
  constructor(messageManager, portID) {
    this.messageManager = messageManager;
    this.portID = portID;
    this.destroyed = false;
    this.listener = new MessageListener();

    // This is a sparse array of pending requests. The id of each request is
    // simply its index in the array. The next id is the current length of the
    // array (which includes the count of missing indexes).
    this.requests = [];

    this.message = this.message.bind(this);
    this.receiveRequest = this.receiveRequest.bind(this);
    this.receiveResponse = this.receiveResponse.bind(this);
    this.addMessageListeners();
  }

  addMessageListeners() {
    this.messageManager.addMessageListener("RemotePage:Message", this.message);
    this.messageManager.addMessageListener(
      "RemotePage:Request",
      this.receiveRequest
    );
    this.messageManager.addMessageListener(
      "RemotePage:Response",
      this.receiveResponse
    );
  }

  removeMessageListeners() {
    this.messageManager.removeMessageListener(
      "RemotePage:Message",
      this.message
    );
    this.messageManager.removeMessageListener(
      "RemotePage:Request",
      this.receiveRequest
    );
    this.messageManager.removeMessageListener(
      "RemotePage:Response",
      this.receiveResponse
    );
  }

  // Called when the message manager used to connect to the other process has
  // changed, i.e. when a tab is detached.
  swapMessageManager(messageManager) {
    this.removeMessageListeners();
    this.messageManager = messageManager;
    this.addMessageListeners();
  }

  // Sends a request to the other process and returns a promise that completes
  // once the other process has responded to the request or some error occurs.
  sendRequest(name, data = null) {
    if (this.destroyed) {
      return this.window.Promise.reject(
        new Error("Message port has been destroyed")
      );
    }

    let deferred = PromiseUtils.defer();
    this.requests.push(deferred);

    this.messageManager.sendAsyncMessage("RemotePage:Request", {
      portID: this.portID,
      requestID: this.requests.length - 1,
      name,
      data,
    });

    return this.wrapPromise(deferred.promise);
  }

  // Handles an IPC message to perform a request of some kind.
  async receiveRequest({ data: messagedata }) {
    if (this.destroyed || messagedata.portID != this.portID) {
      return;
    }

    let data = {
      portID: this.portID,
      requestID: messagedata.requestID,
    };

    try {
      data.resolve = await this.handleRequest(
        messagedata.name,
        messagedata.data
      );
    } catch (e) {
      data.reject = e;
    }

    this.messageManager.sendAsyncMessage("RemotePage:Response", data);
  }

  // Handles an IPC message with the response of a request.
  receiveResponse({ data: messagedata }) {
    if (this.destroyed || messagedata.portID != this.portID) {
      return;
    }

    let deferred = this.requests[messagedata.requestID];
    if (!deferred) {
      Cu.reportError("Received a response to an unknown request.");
      return;
    }

    delete this.requests[messagedata.requestID];

    if ("resolve" in messagedata) {
      deferred.resolve(messagedata.resolve);
    } else if ("reject" in messagedata) {
      deferred.reject(messagedata.reject);
    } else {
      deferred.reject(new Error("Internal RPM error."));
    }
  }

  // Handles an IPC message containing any message.
  message({ data: messagedata }) {
    if (this.destroyed || messagedata.portID != this.portID) {
      return;
    }

    this.handleMessage(messagedata);
  }

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
  }

  /*
   * Removes a listener for messages.
   */
  removeMessageListener(name, callback) {
    if (this.destroyed) {
      throw new Error("Message port has been destroyed");
    }

    this.listener.removeMessageListener(name, callback);
  }

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
  }

  // Called to destroy this port
  destroy() {
    try {
      // This can fail in the child process if the tab has already been closed
      this.removeMessageListeners();
    } catch (e) {}

    for (let deferred of this.requests) {
      if (deferred) {
        deferred.reject(new Error("Message port has been destroyed"));
      }
    }

    this.messageManager = null;
    this.destroyed = true;
    this.portID = null;
    this.listener = null;
    this.requests = [];
  }

  wrapPromise(promise) {
    return new this.window.Promise((resolve, reject) =>
      promise.then(resolve, reject)
    );
  }

  getAppBuildID() {
    let principal = this.window.document.nodePrincipal;
    if (!RPMAccessManager.checkAllowAccess(principal, "getAppBuildID", "yes")) {
      throw new Error(
        "RPMAccessManager does not allow access to getAppBuildID"
      );
    }
    return Services.appinfo.appBuildID;
  }

  getIntPref(aPref, defaultValue) {
    let principal = this.window.document.nodePrincipal;
    if (!RPMAccessManager.checkAllowAccess(principal, "getIntPref", aPref)) {
      throw new Error("RPMAccessManager does not allow access to getIntPref");
    }
    // Only call with a default value if it's defined, to be able to throw
    // errors for non-existent prefs.
    if (defaultValue !== undefined) {
      return Services.prefs.getIntPref(aPref, defaultValue);
    }
    return Services.prefs.getIntPref(aPref);
  }

  getStringPref(aPref) {
    let principal = this.window.document.nodePrincipal;
    if (!RPMAccessManager.checkAllowAccess(principal, "getStringPref", aPref)) {
      throw new Error(
        "RPMAccessManager does not allow access to getStringPref"
      );
    }
    return Services.prefs.getStringPref(aPref);
  }

  getBoolPref(aPref, defaultValue) {
    let principal = this.window.document.nodePrincipal;
    if (!RPMAccessManager.checkAllowAccess(principal, "getBoolPref", aPref)) {
      throw new Error("RPMAccessManager does not allow access to getBoolPref");
    }
    // Only call with a default value if it's defined, to be able to throw
    // errors for non-existent prefs.
    if (defaultValue !== undefined) {
      return Services.prefs.getBoolPref(aPref, defaultValue);
    }
    return Services.prefs.getBoolPref(aPref);
  }

  setBoolPref(aPref, aVal) {
    return this.wrapPromise(AsyncPrefs.set(aPref, aVal));
  }

  getFormatURLPref(aFormatURL) {
    let principal = this.window.document.nodePrincipal;
    if (
      !RPMAccessManager.checkAllowAccess(
        principal,
        "getFormatURLPref",
        aFormatURL
      )
    ) {
      throw new Error(
        "RPMAccessManager does not allow access to getFormatURLPref"
      );
    }
    return Services.urlFormatter.formatURLPref(aFormatURL);
  }

  isWindowPrivate() {
    let principal = this.window.document.nodePrincipal;
    if (
      !RPMAccessManager.checkAllowAccess(principal, "isWindowPrivate", "yes")
    ) {
      throw new Error(
        "RPMAccessManager does not allow access to isWindowPrivate"
      );
    }
    return PrivateBrowsingUtils.isContentWindowPrivate(this.window);
  }

  getUpdateChannel() {
    let principal = this.window.document.nodePrincipal;
    if (
      !RPMAccessManager.checkAllowAccess(principal, "getUpdateChannel", "yes")
    ) {
      throw new Error(
        "RPMAccessManager does not allow access to getUpdateChannel"
      );
    }
    return UpdateUtils.UpdateChannel;
  }

  getFxAccountsEndpoint(aEntrypoint) {
    let principal = this.window.document.nodePrincipal;
    if (
      !RPMAccessManager.checkAllowAccess(
        principal,
        "getFxAccountsEndpoint",
        "yes"
      )
    ) {
      throw new Error(
        "RPMAccessManager does not allow access to getFxAccountsEndpoint"
      );
    }

    return this.sendRequest("FxAccountsEndpoint", aEntrypoint);
  }

  recordTelemetryEvent(category, event, object, value, extra) {
    let principal = this.window.document.nodePrincipal;
    if (
      !RPMAccessManager.checkAllowAccess(
        principal,
        "recordTelemetryEvent",
        "yes"
      )
    ) {
      throw new Error(
        "RPMAccessManager does not allow access to recordTelemetryEvent"
      );
    }
    return Services.telemetry.recordEvent(
      category,
      event,
      object,
      value,
      extra
    );
  }
}
