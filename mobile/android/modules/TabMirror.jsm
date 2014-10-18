/* -*- Mode: js; js-indent-level: 2; indent-tabs-mode: nil; tab-width: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

const CONFIG = { iceServers: [{ "url": "stun:stun.services.mozilla.com" }] };

let log = Cu.import("resource://gre/modules/AndroidLog.jsm",
                    {}).AndroidLog.d.bind(null, "TabMirror");

let failure = function(x) {
  log("ERROR: " + JSON.stringify(x));
};

let TabMirror = function(deviceId, window) {

  this.deviceId = deviceId;
  // Save mozRTCSessionDescription and mozRTCIceCandidate for later when the window object is not available.
  this.RTCSessionDescription = window.mozRTCSessionDescription;
  this.RTCIceCandidate = window.mozRTCIceCandidate;

  Services.obs.addObserver((aSubject, aTopic, aData) => this._processMessage(aData), "MediaPlayer:Response", false);

  this._pc = new window.mozRTCPeerConnection(CONFIG, {});

  if (!this._pc) {
    throw "Failure creating Webrtc object";
  }

  this._pc.onicecandidate = this._onIceCandidate.bind(this);

  let windowId = window.BrowserApp.selectedBrowser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  let viewport = window.BrowserApp.selectedTab.getViewport();
  let maxWidth =  Math.max(viewport.cssWidth, viewport.width);
  let maxHeight = Math.max(viewport.cssHeight, viewport.height);
  let minRatio = Math.sqrt((maxWidth * maxHeight) / (640 * 480));

  let screenWidth = 640;
  let screenHeight = 480;
  let videoWidth = 0;
  let videoHeight = 0;
  if (screenWidth/screenHeight > maxWidth / maxHeight) {
    videoWidth = screenWidth;
    videoHeight = Math.ceil(videoWidth * maxHeight / maxWidth);
  } else {
    videoHeight = screenHeight;
    videoWidth = Math.ceil(videoHeight * maxWidth / maxHeight);
  }

  let constraints = {
    video: {
      mediaSource: "browser",
      browserWindow: windowId,
      scrollWithPage: true,
      advanced: [
        { width: { min: videoWidth, max: videoWidth },
          height: { min: videoHeight, max: videoHeight }
        },
        { aspectRatio: maxWidth / maxHeight }
      ]
    }
  };

  window.navigator.mozGetUserMedia(constraints, this._onGumSuccess.bind(this), this._onGumFailure.bind(this));
};

TabMirror.prototype = {

  _processMessage: function(data) {

    if (!data) {
      return;
    }

    let msg = JSON.parse(data);

    if (!msg) {
      return;
    }

    if (msg.sdp) {
      if (msg.type === "answer") {
        this._processAnswer(msg);
      } else {
        log("Unandled sdp message type: " + msg.type);
      }
    } else {
      this._processIceCandidate(msg);
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
