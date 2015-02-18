/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MatchstickApp"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");

function log(msg) {
  Services.console.logStringMessage(msg);
}

const MATCHSTICK_PLAYER_URL = "http://openflint.github.io/flint-player/player.html";

const STATUS_RETRY_COUNT = 5;   // Number of times we retry a partial status
const STATUS_RETRY_WAIT = 1000; // Delay between attempts in milliseconds

/* MatchstickApp is a wrapper for interacting with a DIAL server.
 * The basic interactions all use a REST API.
 * See: https://github.com/openflint/openflint.github.io/wiki/Flint%20Protocol%20Docs
 */
function MatchstickApp(aServer) {
  this.server = aServer;
  this.app = "~flintplayer";
  this.resourceURL = this.server.appsURL + this.app;
  this.token = null;
  this.statusTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  this.statusRetry = 0;
}

MatchstickApp.prototype = {
  status: function status(aCallback) {
    // Query the server to see if an application is already running
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", this.resourceURL, true);
    xhr.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    xhr.setRequestHeader("Accept", "application/xml; charset=utf8");
    xhr.setRequestHeader("Authorization", this.token);

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200) {
        let doc = xhr.responseXML;
        let state = doc.querySelector("state").textContent;

        // The serviceURL can be missing if the player is not completely loaded
        let serviceURL = null;
        let serviceNode = doc.querySelector("channelBaseUrl");
        if (serviceNode) {
          serviceURL = serviceNode.textContent + "/senders/" + this.token;
        }

        if (aCallback)
          aCallback({ state: state, serviceURL: serviceURL });
      } else {
        if (aCallback)
          aCallback({ state: "error" });
      }
    }).bind(this), false);

    xhr.addEventListener("error", (function() {
      if (aCallback)
        aCallback({ state: "error" });
    }).bind(this), false);

    xhr.send(null);
  },

  start: function start(aCallback) {
    // Start a given app with any extra query data. Each app uses it's own data scheme.
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("POST", this.resourceURL, true);
    xhr.overrideMimeType("text/xml");
    xhr.setRequestHeader("Content-Type", "application/json");

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200 || xhr.status == 201) {
        this.statusRetry = 0;

        let response = JSON.parse(xhr.responseText);
        this.token = response.token;
        this.pingInterval = response.interval;

        if (aCallback)
          aCallback(true);
      } else {
        if (aCallback)
          aCallback(false);
      }
    }).bind(this), false);

    xhr.addEventListener("error", (function() {
      if (aCallback)
        aCallback(false);
    }).bind(this), false);

    let data = {
      type: "launch",
      app_info: {
        url: MATCHSTICK_PLAYER_URL,
        useIpc: true,
        maxInactive: -1
      }
    };

    xhr.send(JSON.stringify(data));
  },

  stop: function stop(aCallback) {
    // Send command to kill an app, if it's already running.
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("DELETE", this.resourceURL + "/run", true);
    xhr.overrideMimeType("text/plain");
    xhr.setRequestHeader("Accept", "application/xml; charset=utf8");
    xhr.setRequestHeader("Authorization", this.token);

    xhr.addEventListener("load", (function() {
      if (xhr.status == 200) {
        if (aCallback)
          aCallback(true);
      } else {
        if (aCallback)
          aCallback(false);
      }
    }).bind(this), false);

    xhr.addEventListener("error", (function() {
      if (aCallback)
        aCallback(false);
    }).bind(this), false);

    xhr.send(null);
  },

  remoteMedia: function remoteMedia(aCallback, aListener) {
    this.status((aStatus) => {
      if (aStatus.serviceURL) {
        if (aCallback) {
          aCallback(new RemoteMedia(aStatus.serviceURL, aListener, this));
        }
        return;
      }

      // It can take a few moments for the player app to load. Let's use a small delay
      // and retry a few times.
      if (this.statusRetry < STATUS_RETRY_COUNT) {
        this.statusRetry++;
        this.statusTimer.initWithCallback(() => {
          this.remoteMedia(aCallback, aListener);
        }, STATUS_RETRY_WAIT, Ci.nsITimer.TYPE_ONE_SHOT);
      } else {
        // Fail
        if (aCallback) {
          aCallback();
        }
      }
    });
  }
}

/* RemoteMedia provides a wrapper for using WebSockets and Flint protocol to control
 * the Matchstick media player
 * See: https://github.com/openflint/openflint.github.io/wiki/Flint%20Protocol%20Docs
 * See: https://github.com/openflint/flint-receiver-sdk/blob/gh-pages/v1/libs/mediaplayer.js
 */
function RemoteMedia(aURL, aListener, aApp) {
  this._active = false;
  this._status = "uninitialized";

  this.app = aApp;
  this.listener = aListener;

  this.pingTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  let uri = Services.io.newURI(aURL, null, null);
  this.ws = Cc["@mozilla.org/network/protocol;1?name=ws"].createInstance(Ci.nsIWebSocketChannel);
  this.ws.initLoadInfo(null, // aLoadingNode
                       Services.scriptSecurityManager.getSystemPrincipal(),
                       null, // aTriggeringPrincipal
                       Ci.nsILoadInfo.SEC_NORMAL,
                       Ci.nsIContentPolicy.TYPE_WEBSOCKET);

  this.ws.asyncOpen(uri, aURL, this, null);
}

// Used to give us a small gap between not pinging too often and pinging too late
const PING_INTERVAL_BACKOFF = 200;

RemoteMedia.prototype = {
  _ping: function _ping() {
    if (this.app.pingInterval == -1) {
      return;
    }

    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", this.app.resourceURL, true);
    xhr.setRequestHeader("Accept", "application/xml; charset=utf8");
    xhr.setRequestHeader("Authorization", this.app.token);

    xhr.addEventListener("load", () => {
      if (xhr.status == 200) {
        this.pingTimer.initWithCallback(() => {
          this._ping();
        }, this.app.pingInterval - PING_INTERVAL_BACKOFF, Ci.nsITimer.TYPE_ONE_SHOT);
      }
    });

    xhr.send(null);
  },

  _changeStatus: function _changeStatus(status) {
    if (this._status != status) {
      this._status = status;
      if ("onRemoteMediaStatus" in this.listener) {
        this.listener.onRemoteMediaStatus(this);
      }
    }
  },

  _teardown: function _teardown() {
    if (!this._active) {
      return;
    }

    // Stop any queued ping event
    this.pingTimer.cancel();

    // Let the listener know we are finished
    this._active = false;
    if (this.listener && "onRemoteMediaStop" in this.listener) {
      this.listener.onRemoteMediaStop(this);
    }
  },

  _sendMsg: function _sendMsg(params) {
    // Convert payload to a string
    params.payload = JSON.stringify(params.payload);

    try {
      this.ws.sendMsg(JSON.stringify(params));
    } catch (e if e.result == Cr.NS_ERROR_NOT_CONNECTED) {
      // This shouldn't happen unless something gets out of sync with the
      // connection. Let's make sure we try to cleanup.
      this._teardown();
    } catch (e) {
      log("Send Error: " + e)
    }
  },

  get active() {
    return this._active;
  },

  get status() {
    return this._status;
  },

  shutdown: function shutdown() {
    this.ws.close(Ci.nsIWebSocketChannel.CLOSE_NORMAL, "shutdown");
  },

  play: function play() {
    if (!this._active) {
      return;
    }

    let params = {
      namespace: "urn:flint:org.openflint.fling.media",
      payload: {
        type: "PLAY",
        requestId: "requestId-5",
      }
    };

    this._sendMsg(params);
  },

  pause: function pause() {
    if (!this._active) {
      return;
    }

    let params = {
      namespace: "urn:flint:org.openflint.fling.media",
      payload: {
        type: "PAUSE",
        requestId: "requestId-4",
      }
    };

    this._sendMsg(params);
  },

  load: function load(aData) {
    if (!this._active) {
      return;
    }

    let params = {
      namespace: "urn:flint:org.openflint.fling.media",
      payload: {
        type: "LOAD",
        requestId: "requestId-2",
        media: {
          contentId: aData.source,
          contentType: "video/mp4",
          metadata: {
            title: "",
            subtitle: ""
          }
        }
      }
    };

    this._sendMsg(params);
  },

  onStart: function(aContext) {
    this._active = true;
    if (this.listener && "onRemoteMediaStart" in this.listener) {
      this.listener.onRemoteMediaStart(this);
    }

    this._ping();
  },

  onStop: function(aContext, aStatusCode) {
    // This will be called for internal socket failures and timeouts. Make
    // sure we cleanup.
    this._teardown();
  },

  onAcknowledge: function(aContext, aSize) {},
  onBinaryMessageAvailable: function(aContext, aMessage) {},

  onMessageAvailable: function(aContext, aMessage) {
    let msg = JSON.parse(aMessage);
    if (!msg) {
      return;
    }

    let payload = JSON.parse(msg.payload);
    if (!payload) {
      return;
    }

    // Handle state changes using the player notifications
    if (payload.type == "MEDIA_STATUS") {
      let status = payload.status[0];
      let state = status.playerState.toLowerCase();
      if (state == "playing") {
        this._changeStatus("started");
      } else if (state == "paused") {
        this._changeStatus("paused");
      } else if (state == "idle" && "idleReason" in status) {
        // Make sure we are really finished. IDLE can be sent at other times.
        let reason = status.idleReason.toLowerCase();
        if (reason == "finished") {
          this._changeStatus("completed");
        }
      }
    }
  },

  onServerClose: function(aContext, aStatusCode, aReason) {
    // This will be fired from _teardown when we close the websocket, but it
    // can also be called for other internal socket failures and timeouts. We
    // make sure the _teardown bails on reentry.
    this._teardown();
  }
}
