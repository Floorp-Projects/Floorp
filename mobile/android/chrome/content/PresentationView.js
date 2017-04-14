/* -*- Mode: tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const TOPIC_PRESENTATION_VIEW_READY = "presentation-view-ready";
const TOPIC_PRESENTATION_RECEIVER_LAUNCH = "presentation-receiver:launch";
const TOPIC_PRESENTATION_RECEIVER_LAUNCH_RESPONSE = "presentation-receiver:launch:response";

// globals Services
Cu.import("resource://gre/modules/Services.jsm");

function log(str) {
  // dump("-*- PresentationView.js -*-: " + str + "\n");
}

let PresentationView = {
  _id: null,

  startup: function startup() {
    // use hash as the ID of this top level window
    this._id = window.location.hash.substr(1);

    // Listen "presentation-receiver:launch" sent from
    // PresentationRequestUIGlue.
    Services.obs.addObserver(this,TOPIC_PRESENTATION_RECEIVER_LAUNCH);

    // Notify PresentationView is ready.
    Services.obs.notifyObservers(null, TOPIC_PRESENTATION_VIEW_READY, this._id);
  },

  stop: function stop() {
    Services.obs.removeObserver(this, TOPIC_PRESENTATION_RECEIVER_LAUNCH);
  },

  observe: function observe(aSubject, aTopic, aData) {
    log("Got observe: aTopic=" + aTopic);

    let requestData = JSON.parse(aData);
    if (this._id != requestData.windowId) {
      return;
    }

    let browser = document.getElementById("content");
    browser.setAttribute("mozpresentation", requestData.url);
    try {
      browser.loadURI(requestData.url);
      Services.obs.notifyObservers(browser,
                                  TOPIC_PRESENTATION_RECEIVER_LAUNCH_RESPONSE,
                                  JSON.stringify({ result: "success",
                                                   requestId: requestData.requestId }));
    } catch (e) {
      Services.obs.notifyObservers(null,
                                   TOPIC_PRESENTATION_RECEIVER_LAUNCH_RESPONSE,
                                   JSON.stringify({ result: "error",
                                                    reason: e.message }));
    }
  }
};
