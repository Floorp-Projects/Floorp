// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PresentationApp"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "sysInfo", () => {
  return Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
});

const DEBUG = false;

const STATE_UNINIT = "uninitialized" // RemoteMedia status
const STATE_STARTED = "started"; // RemoteMedia status
const STATE_PAUSED = "paused"; // RemoteMedia status
const STATE_SHUTDOWN = "shutdown"; // RemoteMedia status

function debug(msg) {
  Services.console.logStringMessage("PresentationApp: " + msg);
}

// PresentationApp is a wrapper for interacting with a Presentation Receiver Device.
function PresentationApp(service, request) {
  this.service = service;
  this.request = request;
}

PresentationApp.prototype = {
  start: function start(callback) {
    this.request.startWithDevice(this.service.uuid)
    .then((session) => {
      this._session = session;
      if (callback) {
        session.addEventListener('connect', () => {
          callback(true);
        });
      }
    }, () => {
      if (callback) {
        callback(false);
      }
    });
  },

  stop: function stop(callback) {
    if (this._session && this._session.state === "connected") {
      this._session.terminate();
    }

    delete this._session;

    if (callback) {
      callback(true);
    }
  },

  remoteMedia: function remoteMedia(callback, listener) {
    if (callback) {
      if (!this._session) {
        callback();
        return;
      }

      callback(new RemoteMedia(this._session, listener));
    }
  }
}

/* RemoteMedia provides a wrapper for using Presentation API to control Firefox TV app.
 * The server implementation must be built into the Firefox TV receiver app.
 * see https://github.com/mozilla-b2g/gaia/tree/master/tv_apps/fling-player
 */
function RemoteMedia(session, listener) {
  this._session = session ;
  this._listener = listener;
  this._status = STATE_UNINIT;

  this._session.addEventListener("message", this);
  this._session.addEventListener("terminate", this);

  if (this._listener && "onRemoteMediaStart" in this._listener) {
    Services.tm.mainThread.dispatch((function() {
      this._listener.onRemoteMediaStart(this);
    }).bind(this), Ci.nsIThread.DISPATCH_NORMAL);
  }
}

RemoteMedia.prototype = {
  _seq: 0,

  handleEvent: function(e) {
    switch (e.type) {
      case "message":
        this._onmessage(e);
        break;
      case "terminate":
        this._onterminate(e);
        break;
    }
  },

  _onmessage: function(e) {
    DEBUG && debug("onmessage: " + e.data);
    if (this.status === STATE_SHUTDOWN) {
      return;
    }

    if (e.data.indexOf("stopped") > -1) {
      if (this.status !== STATE_PAUSED) {
        this._status = STATE_PAUSED;
        if (this._listener && "onRemoteMediaStatus" in this._listener) {
          this._listener.onRemoteMediaStatus(this);
        }
      }
    } else if (e.data.indexOf("playing") > -1) {
      if (this.status !== STATE_STARTED) {
        this._status = STATE_STARTED;
        if (this._listener && "onRemoteMediaStatus" in this._listener) {
          this._listener.onRemoteMediaStatus(this);
        }
      }
    }
  },

  _onterminate: function(e) {
    DEBUG && debug("onterminate: " + this._session.state);
    this._status = STATE_SHUTDOWN;
    if (this._listener && "onRemoteMediaStop" in this._listener) {
      this._listener.onRemoteMediaStop(this);
    }
  },

  _sendCommand: function(command, data) {
    let msg = {
      'type': command,
      'seq': ++this._seq
    };

    if (data) {
      for (var k in data) {
        msg[k] = data[k];
      }
    }

    let raw = JSON.stringify(msg);
    DEBUG && debug("send command: " + raw);

    this._session.send(raw);
  },

  shutdown: function shutdown() {
    DEBUG && debug("RemoteMedia - shutdown");
    this._sendCommand("close");
  },

  play: function play() {
    DEBUG && debug("RemoteMedia - play");
    this._sendCommand("play");
  },

  pause: function pause() {
    DEBUG && debug("RemoteMedia - pause");
    this._sendCommand("pause");
  },

  load: function load(data) {
    DEBUG && debug("RemoteMedia - load: " + data);
    this._sendCommand("load", { "url": data.source });

    let deviceName;
    if (Services.appinfo.widgetToolkit == "android") {
      deviceName = sysInfo.get("device");
    } else {
      deviceName = sysInfo.get("host");
    }
    this._sendCommand("device-info", { "displayName": deviceName });
  },

  get status() {
    return this._status;
  }
}
