/* -*- Mode: js; js-indent-level: 2; indent-tabs-mode: nil; tab-width: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

const CONFIG = { iceServers: [{ "urls": ["stun:stun.services.mozilla.com"] }] };

var log = Cu.import("resource://gre/modules/AndroidLog.jsm",
                    {}).AndroidLog.d.bind(null, "TabMirror");

var failure = function(x) {
  log("ERROR: " + JSON.stringify(x));
};

var TabMirror = function(deviceId, window) {

  this.deviceId = deviceId;
  // Save RTCSessionDescription and RTCIceCandidate for later when the window object is not available.
  this.RTCSessionDescription = window.RTCSessionDescription;
  this.RTCIceCandidate = window.RTCIceCandidate;

  Services.obs.addObserver((aSubject, aTopic, aData) => this._processMessage(aData), "MediaPlayer:Response", false);
  this._sendMessage({ start: true });
  this._window = window;
  this._pc = new window.RTCPeerConnection(CONFIG, {});
  if (!this._pc) {
    throw "Failure creating Webrtc object";
  }

};

TabMirror.prototype = {
  _window: null,
  _screenSize: { width: 1280, height: 720 },
  _pc: null,
  _start: function() {
    this._pc.onicecandidate = this._onIceCandidate.bind(this);

    let windowId = this._window.BrowserApp.selectedBrowser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
    let constraints = {
      video: {
        mediaSource: "browser",
        browserWindow: windowId,
        scrollWithPage: true,
        advanced: [
          { width: { min: 0, max: this._screenSize.width },
            height: { min: 0, max: this._screenSize.height }
          },
          { aspectRatio: this._screenSize.width / this._screenSize.height }
        ]
      }
    };

    this._window.navigator.mozGetUserMedia(constraints, this._onGumSuccess.bind(this), this._onGumFailure.bind(this));
  },

  _processMessage: function(data) {
    if (!data) {
      return;
    }

    let msg = JSON.parse(data);

    if (!msg) {
      return;
    }

    if (msg.sdp && msg.type === "answer") {
      this._processAnswer(msg);
    } else if (msg.type == "size") {
      if (msg.height) {
        this._screenSize.height = msg.height;
      }
      if (msg.width) {
        this._screenSize.width = msg.width;
      }
      this._start();
    } else if (msg.candidate) {
      this._processIceCandidate(msg);
    } else {
      log("dropping unrecognized message: " + JSON.stringify(msg));
    }
  },

  // Signaling methods
  _processAnswer: function(msg) {
    this._pc.setRemoteDescription(new this.RTCSessionDescription(msg),
                                  this._setRemoteAnswerSuccess.bind(this), failure);
  },

  _processIceCandidate: function(msg) {
    // WebRTC generates a warning if the success and fail callbacks are not passed in.
    this._pc.addIceCandidate(new this.RTCIceCandidate(msg), () => log("Ice Candiated added successfuly"), () => log("Failed to add Ice Candidate"));
  },

  _setRemoteAnswerSuccess: function() {
  },

  _setLocalSuccessOffer: function(sdp) {
    this._sendMessage(sdp);
  },

  _createOfferSuccess: function(sdp) {
    this._pc.setLocalDescription(sdp, () => this._setLocalSuccessOffer(sdp), failure);
  },

  _onIceCandidate: function (msg) {
    log("NEW Ice Candidate: " + JSON.stringify(msg.candidate));
    this._sendMessage(msg.candidate);
  },

  _ready: function() {
    this._pc.createOffer(this._createOfferSuccess.bind(this), failure);
  },

  _onGumSuccess: function(stream){
    this._pc.addStream(stream);
    this._ready();
  },

  _onGumFailure: function() {
    log("Could not get video stream");
    this._pc.close();
  },

  _sendMessage: function(msg) {
    if (this.deviceId) {
      let obj = {
        type: "MediaPlayer:Message",
        id: this.deviceId,
        data: JSON.stringify(msg)
      };
      Messaging.sendRequest(obj);
    }
  },

  stop: function() {
    if (this.deviceId) {
      let obj = {
        type: "MediaPlayer:End",
        id: this.deviceId
      };
      Services.androidBridge.handleGeckoMessage(obj);
    }
  },
};


this.EXPORTED_SYMBOLS = ["TabMirror"];
