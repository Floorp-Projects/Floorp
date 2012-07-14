/* -*- Mode: JavaScript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");

const EXPORTED_SYMBOLS = ["WorkerAPI"];

function WorkerAPI(provider, port) {
  if (!port)
    throw new Error("Can't initialize WorkerAPI with a null port");

  this._provider = provider;
  this._port = port;
  this._port.onmessage = this._handleMessage.bind(this);

  this.initialized = false;

  // Send an "intro" message so the worker knows this is the port
  // used for the api.
  // later we might even include an API version - version 0 for now!
  this._port.postMessage({topic: "social.initialize"});
}

WorkerAPI.prototype = {
  _handleMessage: function _handleMessage(event) {
    let {topic, data} = event.data;
    let handler = this.handlers[topic];
    if (!handler) {
      Cu.reportError("WorkerAPI: topic doesn't have a handler: '" + topic + "'");
      return;
    }
    try {
      handler.call(this, data);
    } catch (ex) {
      Cu.reportError("WorkerAPI: failed to handle message '" + topic + "': " + ex);
    }
  },

  handlers: {
    "social.initialize-response": function (data) {
      this.initialized = true;
    },
    "social.user-profile": function (data) {
      this._provider.updateUserProfile(data);
    },
    "social.ambient-notification": function (data) {
      this._provider.setAmbientNotification(data);
    },
    
    // XXX backwards compat for existing providers, remove these eventually
    "social.ambient-notification-area": function (data) {
      // replaced with social.user-profile
      // handle the provider icon and user profile for the primary provider menu
      if (data.background) {
        // backwards compat
        try {
          data.iconURL = /url\((['"]?)(.*)(\1)\)/.exec(data.background)[2];
        } catch(e) {
          data.iconURL = data.background;
        }
      }

      this._provider.updateUserProfile(data);
    },
    "social.ambient-notification-update": function (data) {
      // replaced with social.ambient-notification
      // handle the provider icon and user profile for the primary provider menu
      if (data.background) {
        // backwards compat
        try {
          data.iconURL = /url\((['"]?)(.*)(\1)\)/.exec(data.background)[2];
        } catch(e) {
          data.iconURL = data.background;
        }
      }
      this._provider.setAmbientNotification(data);
    }
  }
}
