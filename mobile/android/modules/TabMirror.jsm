/* -*- Mode: js; js-indent-level: 2; indent-tabs-mode: nil; tab-width: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/Services.jsm");

let TabMirror = function(deviceId, window) {
  let out_queue = [];
  let in_queue = [];
  let DEBUG = false;

  let log = Cu.import("resource://gre/modules/AndroidLog.jsm",
                      {}).AndroidLog.d.bind(null, "TabMirror");

  let RTCPeerConnection = window.mozRTCPeerConnection;
  let RTCSessionDescription = window.mozRTCSessionDescription;
  let RTCIceCandidate = window.mozRTCIceCandidate;
  let getUserMedia = window.navigator.mozGetUserMedia.bind(window.navigator);

  Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
    in_queue.push(aData);
  }, "MediaPlayer:Response", false);

  let poll_timeout = 1000; // ms

  let audio_stream = undefined;
  let video_stream = undefined;
  let pc = undefined;

  let poll_success = function(msg) {
    if (!msg) {
      poll_error();
      return;
    }

    let js;
    try {
      js = JSON.parse(msg);
    } catch(ex) {
      log("ex: " + ex);
    }

    let sdp = js.body;

    if (sdp) {
      if (sdp.sdp) {
        if (sdp.type === "offer") {
          process_offer(sdp);
        } else if (sdp.type === "answer") {
          process_answer(sdp);
        }
      } else {
        process_ice_candidate(sdp);
      }
    }

    window.setTimeout(poll, poll_timeout);
  };

  let poll_error = function (msg) {
    window.setTimeout(poll, poll_timeout);
  };

  let poll = function () {
    if (in_queue) {
      poll_success(in_queue.pop());
    }
  };

  let failure = function(x) {
    log("ERROR: " + JSON.stringify(x));
  };


  // Signaling methods
  let send_sdpdescription= function(sdp) {
    let msg = {
      body: sdp
    };

    sendMessage(JSON.stringify(msg));
  };


  let deobjify = function(x) {
    return JSON.parse(JSON.stringify(x));
  };


  let process_offer = function(sdp) {
    pc.setRemoteDescription(new RTCSessionDescription(sdp),
                            set_remote_offer_success, failure);
  };

  let process_answer = function(sdp) {
    pc.setRemoteDescription(new RTCSessionDescription(sdp),
                            set_remote_answer_success, failure);
  };

  let process_ice_candidate = function(msg) {
    pc.addIceCandidate(new RTCIceCandidate(msg));
  };

  let set_remote_offer_success = function() {
    pc.createAnswer(create_answer_success, failure);
  };

  let set_remote_answer_success= function() {
  };

  let set_local_success_offer = function(sdp) {
    send_sdpdescription(sdp);
  };

  let set_local_success_answer = function(sdp) {
    send_sdpdescription(sdp);
  };

  let filter_nonrelay_candidates = function(sdp) {
    let lines = sdp.sdp.split("\r\n");
    let lines2 = lines.filter(function(x) {
      if (!/candidate/.exec(x))
        return true;
      if (/relay/.exec(x))
        return true;

      return false;
    });

    sdp.sdp = lines2.join("\r\n");
  };

  let create_offer_success = function(sdp) {
    pc.setLocalDescription(sdp,
                           function() {
                             set_local_success_offer(sdp);
                           },
                           failure);
  };

  let create_answer_success = function(sdp) {
    pc.setLocalDescription(sdp,
                           function() {
                             set_local_success_answer(sdp);
                           },
                           failure);
  };

  let on_ice_candidate = function (candidate) {
    send_sdpdescription(candidate.candidate);
  };

  let ready = function() {
    pc.createOffer(create_offer_success, failure);
    poll();
  };


  let config = {
    iceServers: [{ "url": "stun:stun.services.mozilla.com" }]
  };

  let pc = new RTCPeerConnection(config, {});

  if (!pc) {
    log("Failure creating Webrtc object");
    return;
  }

  pc.onicecandidate = on_ice_candidate;

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
      advanced: [
        { width: { min: videoWidth, max: videoWidth },
          height: { min: videoHeight, max: videoHeight }
        },
        { aspectRatio: maxWidth / maxHeight }
      ]
    }
  };

  let gUM_success = function(stream){
    pc.addStream(stream);
    ready();
  };

  let gUM_failure = function() {
    log("Could not get video stream");
  };

  getUserMedia( constraints, gUM_success, gUM_failure);

  function sendMessage(msg) {
    let obj = {
      type: "MediaPlayer:Message",
      id: deviceId,
      data: msg
    };

    if (deviceId) {
      Services.androidBridge.handleGeckoMessage(obj);
    }
  }
};


this.EXPORTED_SYMBOLS = ["TabMirror"];
