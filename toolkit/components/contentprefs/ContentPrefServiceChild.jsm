/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "ContentPrefServiceChild" ];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/ContentPrefUtils.jsm");
Cu.import("resource://gre/modules/ContentPrefStore.jsm");

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
  handleResult: function(contentPref) {
    cbHandleResult(this._callback,
                   new ContentPref(contentPref.domain,
                                   contentPref.name,
                                   contentPref.value));
  },

  handleError: function(result) {
    cbHandleError(this._callback, result);
  },

  handleCompletion: function(reason) {
    cbHandleCompletion(this._callback, reason);
  },
};

var ContentPrefServiceChild = {
  QueryInterface: XPCOMUtils.generateQI([ Ci.nsIContentPrefService2 ]),

  // Map from pref name -> set of observers
  _observers: new Map(),

  _mm: Cc["@mozilla.org/childprocessmessagemanager;1"]
         .getService(Ci.nsIMessageSender),

  _getRandomId: function() {
    return Cc["@mozilla.org/uuid-generator;1"]
             .getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  },

  // Map from random ID string -> CallbackCaller, per request
  _requests: new Map(),

  init: function() {
    this._mm.addMessageListener("ContentPrefs:HandleResult", this);
    this._mm.addMessageListener("ContentPrefs:HandleError", this);
    this._mm.addMessageListener("ContentPrefs:HandleCompletion", this);
  },

  receiveMessage: function(msg) {
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

  _callFunction: function(call, args, callback) {
    let requestId = this._getRandomId();
    let data = { call: call, args: args, requestId: requestId };

    this._mm.sendAsyncMessage("ContentPrefs:FunctionCall", data);

    this._requests.set(requestId, new CallbackCaller(callback));
  },

  getByName: function(name, context, callback) {
    return this._callFunction("getByName",
                              [ name, contextArg(context) ],
                              callback);
  },

  getByDomainAndName: function(domain, name, context, callback) {
    return this._callFunction("getByDomainAndName",
                              [ domain, name, contextArg(context) ],
                              callback);
  },

  getBySubdomainAndName: function(domain, name, context, callback) {
    return this._callFunction("getBySubdomainAndName",
                              [ domain, name, contextArg(context) ],
                              callback);
  },

  getGlobal: function(name, context, callback) {
    return this._callFunction("getGlobal",
                              [ name, contextArg(context) ],
                              callback);
  },

  getCachedByDomainAndName: NYI,
  getCachedBySubdomainAndName: NYI,
  getCachedGlobal: NYI,

  set: function(domain, name, value, context, callback) {
    this._callFunction("set",
                       [ domain, name, value, contextArg(context) ],
                       callback);
  },

  setGlobal: function(name, value, context, callback) {
    this._callFunction("setGlobal",
                       [ name, value, contextArg(context) ],
                       callback);
  },

  removeByDomainAndName: function(domain, name, context, callback) {
    this._callFunction("removeByDomainAndName",
                       [ domain, name, contextArg(context) ],
                       callback);
  },

  removeBySubdomainAndName: function(domain, name, context, callback) {
    this._callFunction("removeBySubdomainAndName",
                       [ domain, name, contextArg(context) ],
                       callback);
  },

  removeGlobal: function(name, context, callback) {
    this._callFunction("removeGlobal", [ name, contextArg(context) ], callback);
  },

  removeByDomain: function(domain, context, callback) {
    this._callFunction("removeByDomain", [ domain, contextArg(context) ],
                       callback);
  },

  removeBySubdomain: function(domain, context, callback) {
    this._callFunction("removeBySubdomain", [ domain, contextArg(context) ],
                       callback);
  },

  removeByName: function(name, context, callback) {
    this._callFunction("removeByName", [ name, value, contextArg(context) ],
                       callback);
  },

  removeAllDomains: function(context, callback) {
    this._callFunction("removeAllDomains", [ contextArg(context) ], callback);
  },

  removeAllGlobals: function(context, callback) {
    this._callFunction("removeAllGlobals", [ contextArg(context) ],
                       callback);
  },

  addObserverForName: function(name, observer) {
    let set = this._observers.get(name);
    if (!set) {
      set = new Set();
      if (this._observers.size === 0) {
        // This is the first observer of any kind. Start listening for changes.
        this._mm.addMessageListener("ContentPrefs:NotifyObservers", this);
      }

      // This is the first observer for this name. Start listening for changes
      // to it.
      this._mm.sendAsyncMessage("ContentPrefs:AddObserverForName", { name: name });
      this._observers.set(name, set);
    }

    set.add(observer);
  },

  removeObserverForName: function(name, observer) {
    let set = this._observers.get(name);
    if (!set)
      return;

    set.delete(observer);
    if (set.size === 0) {
      // This was the last observer for this name. Stop listening for changes.
      this._mm.sendAsyncMessage("ContentPrefs:RemoveObserverForName", { name: name });

      this._observers.delete(name);
      if (this._observers.size === 0) {
        // This was the last observer for this process. Stop listing for all
        // changes.
        this._mm.removeMessageListener("ContentPrefs:NotifyObservers", this);
      }
    }
  },

  extractDomain: NYI
};

ContentPrefServiceChild.init();
