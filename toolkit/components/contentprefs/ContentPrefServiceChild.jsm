/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [ "ContentPrefServiceChild" ];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/ContentPrefUtils.jsm");
ChromeUtils.import("resource://gre/modules/ContentPrefStore.jsm");

// We only need one bit of information out of the context.
function contextArg(context) {
  return (context && context.usePrivateBrowsing) ?
            { usePrivateBrowsing: true } :
            null;
}

function NYI() {
  throw new Error("Do not add any new users of these functions");
}

function CallbackCaller(callback) {
  this._callback = callback;
}

CallbackCaller.prototype = {
  handleResult(contentPref) {
    cbHandleResult(this._callback,
                   new ContentPref(contentPref.domain,
                                   contentPref.name,
                                   contentPref.value));
  },

  handleError(result) {
    cbHandleError(this._callback, result);
  },

  handleCompletion(reason) {
    cbHandleCompletion(this._callback, reason);
  },
};

var ContentPrefServiceChild = {
  QueryInterface: ChromeUtils.generateQI([ Ci.nsIContentPrefService2 ]),

  // Map from pref name -> set of observers
  _observers: new Map(),

  _getRandomId() {
    return Cc["@mozilla.org/uuid-generator;1"]
             .getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  },

  // Map from random ID string -> CallbackCaller, per request
  _requests: new Map(),

  init() {
    Services.cpmm.addMessageListener("ContentPrefs:HandleResult", this);
    Services.cpmm.addMessageListener("ContentPrefs:HandleError", this);
    Services.cpmm.addMessageListener("ContentPrefs:HandleCompletion", this);
    Services.obs.addObserver(this, "xpcom-shutdown");
  },

  observe() {
    Services.obs.removeObserver(this, "xpcom-shutdown");
    delete this._observers;
    delete this._requests;
  },

  receiveMessage(msg) {
    let data = msg.data;
    let callback;
    switch (msg.name) {
      case "ContentPrefs:HandleResult":
        callback = this._requests.get(data.requestId);
        callback.handleResult(data.contentPref);
        break;

      case "ContentPrefs:HandleError":
        callback = this._requests.get(data.requestId);
        callback.handleError(data.error);
        break;

      case "ContentPrefs:HandleCompletion":
        callback = this._requests.get(data.requestId);
        this._requests.delete(data.requestId);
        callback.handleCompletion(data.reason);
        break;

      case "ContentPrefs:NotifyObservers": {
        let observerList = this._observers.get(data.name);
        if (!observerList)
          break;

        for (let observer of observerList) {
          safeCallback(observer, data.callback, data.args);
        }

        break;
      }
    }
  },

  _callFunction(call, args, callback) {
    let requestId = this._getRandomId();
    let data = { call, args, requestId };

    Services.cpmm.sendAsyncMessage("ContentPrefs:FunctionCall", data);

    this._requests.set(requestId, new CallbackCaller(callback));
  },

  getCachedByDomainAndName: NYI,
  getCachedBySubdomainAndName: NYI,
  getCachedGlobal: NYI,

  addObserverForName(name, observer) {
    let set = this._observers.get(name);
    if (!set) {
      set = new Set();
      if (this._observers.size === 0) {
        // This is the first observer of any kind. Start listening for changes.
        Services.cpmm.addMessageListener("ContentPrefs:NotifyObservers", this);
      }

      // This is the first observer for this name. Start listening for changes
      // to it.
      Services.cpmm.sendAsyncMessage("ContentPrefs:AddObserverForName", { name });
      this._observers.set(name, set);
    }

    set.add(observer);
  },

  removeObserverForName(name, observer) {
    let set = this._observers.get(name);
    if (!set)
      return;

    set.delete(observer);
    if (set.size === 0) {
      // This was the last observer for this name. Stop listening for changes.
      Services.cpmm.sendAsyncMessage("ContentPrefs:RemoveObserverForName", { name });

      this._observers.delete(name);
      if (this._observers.size === 0) {
        // This was the last observer for this process. Stop listing for all
        // changes.
        Services.cpmm.removeMessageListener("ContentPrefs:NotifyObservers", this);
      }
    }
  },

  extractDomain: NYI
};

function forwardMethodToParent(method, signature, ...args) {
  // Ignore superfluous arguments
  args = args.slice(0, signature.length);

  // Process context argument for forwarding
  let contextIndex = signature.indexOf("context");
  if (contextIndex > -1) {
    args[contextIndex] = contextArg(args[contextIndex]);
  }
  // Take out the callback argument, if present.
  let callbackIndex = signature.indexOf("callback");
  let callback = null;
  if (callbackIndex > -1 && args.length > callbackIndex) {
    callback = args.splice(callbackIndex, 1)[0];
  }
  this._callFunction(method, args, callback);
}

for (let [method, signature] of _methodsCallableFromChild) {
  ContentPrefServiceChild[method] = forwardMethodToParent.bind(ContentPrefServiceChild, method, signature);
}

ContentPrefServiceChild.init();
