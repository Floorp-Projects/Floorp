/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

const MSG_PROMISE_REQUEST  = "WebAPIPromiseRequest";
const MSG_PROMISE_RESULT   = "WebAPIPromiseResult";
const MSG_INSTALL_EVENT    = "WebAPIInstallEvent";
const MSG_INSTALL_CLEANUP  = "WebAPICleanup";
const MSG_ADDON_EVENT_REQ  = "WebAPIAddonEventRequest";
const MSG_ADDON_EVENT      = "WebAPIAddonEvent";

const APIBroker = {
  _nextID: 0,

  init() {
    this._promises = new Map();

    // _installMap maps integer ids to DOM AddonInstall instances
    this._installMap = new Map();

    Services.cpmm.addMessageListener(MSG_PROMISE_RESULT, this);
    Services.cpmm.addMessageListener(MSG_INSTALL_EVENT, this);

    this._eventListener = null;
  },

  receiveMessage(message) {
    let payload = message.data;

    switch (message.name) {
      case MSG_PROMISE_RESULT: {
        if (!this._promises.has(payload.callbackID)) {
          return;
        }

        let { resolve, reject } = this._promises.get(payload.callbackID);
        this._promises.delete(payload.callbackID);

        if ("resolve" in payload)
          resolve(payload.resolve);
        else
          reject(payload.reject);
        break;
      }

      case MSG_INSTALL_EVENT: {
        let install = this._installMap.get(payload.id);
        if (!install) {
          let err = new Error(`Got install event for unknown install ${payload.id}`);
          Cu.reportError(err);
          return;
        }
        install._dispatch(payload);
        break;
      }

      case MSG_ADDON_EVENT: {
        if (this._eventListener) {
          this._eventListener(payload);
        }
      }
    }
  },

  sendRequest: function(type, ...args) {
    return new Promise((resolve, reject) => {
      let callbackID = this._nextID++;

      this._promises.set(callbackID, { resolve, reject });
      Services.cpmm.sendAsyncMessage(MSG_PROMISE_REQUEST, { type, callbackID, args });
    });
  },

  setAddonListener(callback) {
    this._eventListener = callback;
    if (callback) {
      Services.cpmm.addMessageListener(MSG_ADDON_EVENT, this);
      Services.cpmm.sendAsyncMessage(MSG_ADDON_EVENT_REQ, {enabled: true});
    } else {
      Services.cpmm.removeMessageListener(MSG_ADDON_EVENT, this);
      Services.cpmm.sendAsyncMessage(MSG_ADDON_EVENT_REQ, {enabled: false});
    }
  },

  sendCleanup: function(ids) {
    this.setAddonListener(null);
    Services.cpmm.sendAsyncMessage(MSG_INSTALL_CLEANUP, { ids });
  },
};

APIBroker.init();

function Addon(window, properties) {
  this.window = window;

  // We trust the webidl binding to broker access to our properties.
  for (let key of Object.keys(properties)) {
    this[key] = properties[key];
  }
}

function AddonInstall(window, properties) {
  let id = properties.id;
  APIBroker._installMap.set(id, this);

  this.window = window;
  this.handlers = new Map();

  for (let key of Object.keys(properties)) {
    this[key] = properties[key];
  }
}

/**
 * API methods should return promises from the page, this is a simple wrapper
 * to make sure of that. It also automatically wraps objects when necessary.
 */
function WebAPITask(generator) {
  return function(...args) {
    let task = Task.async(generator.bind(this));

    let win = this.window;

    let wrapForContent = (obj) => {
      if (obj instanceof Addon) {
        return win.Addon._create(win, obj);
      }
      if (obj instanceof AddonInstall) {
        return win.AddonInstall._create(win, obj);
      }

      return obj;
    }

    return new win.Promise((resolve, reject) => {
      task(...args).then(wrapForContent)
                   .then(resolve, reject);
    });
  }
}

Addon.prototype = {
  uninstall: WebAPITask(function*() {
    return yield APIBroker.sendRequest("addonUninstall", this.id);
  }),
};

const INSTALL_EVENTS = [
  "onDownloadStarted",
  "onDownloadProgress",
  "onDownloadEnded",
  "onDownloadCancelled",
  "onDownloadFailed",
  "onInstallStarted",
  "onInstallEnded",
  "onInstallCancelled",
  "onInstallFailed",
];

AddonInstall.prototype = {
  _dispatch(data) {
    // The message for the event includes updated copies of all install
    // properties.  Use the usual "let webidl filter visible properties" trick.
    for (let key of Object.keys(data)) {
      this[key] = data[key];
    }

    let event = new this.window.Event(data.event);
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  install: WebAPITask(function*() {
    yield APIBroker.sendRequest("addonInstallDoInstall", this.id);
  }),

  cancel: WebAPITask(function*() {
    yield APIBroker.sendRequest("addonInstallCancel", this.id);
  }),
};

function WebAPI() {
}

WebAPI.prototype = {
  init(window) {
    this.window = window;
    this.allInstalls = [];
    this.listenerCount = 0;

    window.addEventListener("unload", event => {
      APIBroker.sendCleanup(this.allInstalls);
    });
  },

  getAddonByID: WebAPITask(function*(id) {
    let addonInfo = yield APIBroker.sendRequest("getAddonByID", id);
    return addonInfo ? new Addon(this.window, addonInfo) : null;
  }),

  createInstall: WebAPITask(function*(options) {
    let installInfo = yield APIBroker.sendRequest("createInstall", options);
    if (!installInfo) {
      return null;
    }
    let install = new AddonInstall(this.window, installInfo);
    this.allInstalls.push(installInfo.id);
    return install;
  }),

  eventListenerWasAdded(type) {
    if (this.listenerCount == 0) {
      APIBroker.setAddonListener(data => {
        let event = new this.window.AddonEvent(data.event, data);
        this.__DOM_IMPL__.dispatchEvent(event);
      });
    }
    this.listenerCount++;
  },

  eventListenerWasRemoved(type) {
    this.listenerCount--;
    if (this.listenerCount == 0) {
      APIBroker.setAddonListener(null);
    }
  },

  classID: Components.ID("{8866d8e3-4ea5-48b7-a891-13ba0ac15235}"),
  contractID: "@mozilla.org/addon-web-api/manager;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIDOMGlobalPropertyInitializer])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WebAPI]);
