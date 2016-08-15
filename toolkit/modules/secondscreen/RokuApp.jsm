// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["RokuApp"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

function log(msg) {
  //Services.console.logStringMessage(msg);
}

const PROTOCOL_VERSION = 1;

/* RokuApp is a wrapper for interacting with a Roku channel.
 * The basic interactions all use a REST API.
 * spec: http://sdkdocs.roku.com/display/sdkdoc/External+Control+Guide
 */
function RokuApp(service) {
  this.service = service;
  this.resourceURL = this.service.location;
  this.app = AppConstants.RELEASE_BUILD ? "Firefox" : "Firefox Nightly";
  this.mediaAppID = -1;
}

RokuApp.prototype = {
  status: function status(callback) {
    // We have no way to know if the app is running, so just return "unknown"
    // but we use this call to fetch the mediaAppID for the given app name
    let url = this.resourceURL + "query/apps";
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", url, true);
    xhr.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    xhr.overrideMimeType("text/xml");

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200) {
        let doc = xhr.responseXML;
        let apps = doc.querySelectorAll("app");
        for (let app of apps) {
          if (app.textContent == this.app) {
            this.mediaAppID = app.id;
          }
        }
      }

      // Since ECP has no way of telling us if an app is running, we always return "unknown"
      if (callback) {
        callback({ state: "unknown" });
      }
    }).bind(this), false);

    xhr.addEventListener("error", (function() {
      if (callback) {
        callback({ state: "unknown" });
      }
    }).bind(this), false);

    xhr.send(null);
  },

  start: function start(callback) {
    // We need to make sure we have cached the mediaAppID
    if (this.mediaAppID == -1) {
      this.status(function() {
        // If we found the mediaAppID, use it to make a new start call
        if (this.mediaAppID != -1) {
          this.start(callback);
        } else {
          // We failed to start the app, so let the caller know
          callback(false);
        }
      }.bind(this));
      return;
    }

    // Start a given app with any extra query data. Each app uses it's own data scheme.
    // NOTE: Roku will also pass "source=external-control" as a param
    let url = this.resourceURL + "launch/" + this.mediaAppID + "?version=" + parseInt(PROTOCOL_VERSION);
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("POST", url, true);
    xhr.overrideMimeType("text/plain");

    xhr.addEventListener("load", (function() {
      if (callback) {
        callback(xhr.status === 200);
      }
    }).bind(this), false);

    xhr.addEventListener("error", (function() {
      if (callback) {
        callback(false);
      }
    }).bind(this), false);

    xhr.send(null);
  },

  stop: function stop(callback) {
    // Roku doesn't seem to support stopping an app, so let's just go back to
    // the Home screen
    let url = this.resourceURL + "keypress/Home";
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("POST", url, true);
    xhr.overrideMimeType("text/plain");

    xhr.addEventListener("load", (function() {
      if (callback) {
        callback(xhr.status === 200);
      }
    }).bind(this), false);

    xhr.addEventListener("error", (function() {
      if (callback) {
        callback(false);
      }
    }).bind(this), false);

    xhr.send(null);
  },

  remoteMedia: function remoteMedia(callback, listener) {
    if (this.mediaAppID != -1) {
      if (callback) {
        callback(new RemoteMedia(this.resourceURL, listener));
      }
    } else if (callback) {
      callback();
    }
  }
}

/* RemoteMedia provides a wrapper for using TCP socket to control Roku apps.
 * The server implementation must be built into the Roku receiver app.
 */
function RemoteMedia(url, listener) {
  this._url = url;
  this._listener = listener;
  this._status = "uninitialized";

  let serverURI = Services.io.newURI(this._url , null, null);
  this._socket = Cc["@mozilla.org/network/socket-transport-service;1"].getService(Ci.nsISocketTransportService).createTransport(null, 0, serverURI.host, 9191, null);
  this._outputStream = this._socket.openOutputStream(0, 0, 0);

  this._scriptableStream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(Ci.nsIScriptableInputStream);

  this._inputStream = this._socket.openInputStream(0, 0, 0);
  this._pump = Cc["@mozilla.org/network/input-stream-pump;1"].createInstance(Ci.nsIInputStreamPump);
  this._pump.init(this._inputStream, -1, -1, 0, 0, true);
  this._pump.asyncRead(this, null);
}

RemoteMedia.prototype = {
  onStartRequest: function(request, context) {
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    this._scriptableStream.init(stream);
    let data = this._scriptableStream.read(count);
    if (!data) {
      return;
    }

    let msg = JSON.parse(data);
    if (this._status === msg._s) {
      return;
    }

    this._status = msg._s;

    if (this._listener) {
      // Check to see if we are getting the initial "connected" message
      if (this._status == "connected" && "onRemoteMediaStart" in this._listener) {
        this._listener.onRemoteMediaStart(this);
      }

      if ("onRemoteMediaStatus" in this._listener) {
        this._listener.onRemoteMediaStatus(this);
      }
    }
  },

  onStopRequest: function(request, context, result) {
    if (this._listener && "onRemoteMediaStop" in this._listener)
      this._listener.onRemoteMediaStop(this);
  },

  _sendMsg: function _sendMsg(data) {
    if (!data)
      return;

    // Add the protocol version
    data["_v"] = PROTOCOL_VERSION;

    let raw = JSON.stringify(data);
    this._outputStream.write(raw, raw.length);
  },

  shutdown: function shutdown() {
    this._outputStream.close();
    this._inputStream.close();
  },

  get active() {
    return (this._socket && this._socket.isAlive());
  },

  play: function play() {
    // TODO: add position support
    this._sendMsg({ type: "PLAY" });
  },

  pause: function pause() {
    this._sendMsg({ type: "STOP" });
  },

  load: function load(data) {
    this._sendMsg({ type: "LOAD", title: data.title, source: data.source, poster: data.poster });
  },

  get status() {
    return this._status;
  }
}
