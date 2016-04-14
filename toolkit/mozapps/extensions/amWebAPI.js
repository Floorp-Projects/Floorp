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

const APIBroker = {
  _nextID: 0,

  init() {
    this._promises = new Map();

    Services.cpmm.addMessageListener(MSG_PROMISE_RESULT, this);
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
    }
  },

  sendRequest: function(type, ...args) {
    return new Promise((resolve, reject) => {
      let callbackID = this._nextID++;

      this._promises.set(callbackID, { resolve, reject });
      Services.cpmm.sendAsyncMessage(MSG_PROMISE_REQUEST, { type, callbackID, args });
    });
  },
};

APIBroker.init();

function Addon(properties) {
  // We trust the webidl binding to broker access to our properties.
  for (let key of Object.keys(properties)) {
    this[key] = properties[key];
  }
}

/**
 * API methods should return promises from the page, this is a simple wrapper
 * to make sure of that. It also automatically wraps objects when necessary.
 */
function WebAPITask(generator) {
  let task = Task.async(generator);

  return function(...args) {
    let win = this.window;

    let wrapForContent = (obj) => {
      if (obj instanceof Addon) {
        return win.Addon._create(win, obj);
      }

      return obj;
    }

    return new win.Promise((resolve, reject) => {
      task(...args).then(wrapForContent)
                   .then(resolve, reject);
    });
  }
}

function WebAPI() {
}

WebAPI.prototype = {
  init(window) {
    this.window = window;
  },

  getAddonByID: WebAPITask(function*(id) {
    let addonInfo = yield APIBroker.sendRequest("getAddonByID", id);
    return addonInfo ? new Addon(addonInfo) : null;
  }),

  classID: Components.ID("{8866d8e3-4ea5-48b7-a891-13ba0ac15235}"),
  contractID: "@mozilla.org/addon-web-api/manager;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIDOMGlobalPropertyInitializer])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WebAPI]);
