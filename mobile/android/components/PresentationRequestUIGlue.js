/* -*- Mode: tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { interfaces: Ci, utils: Cu, classes: Cc } = Components;

const TOPIC_PRESENTATION_RECEIVER_LAUNCH = "presentation-receiver:launch";
const TOPIC_PRESENTATION_RECEIVER_LAUNCH_RESPONSE = "presentation-receiver:launch:response";

// globals XPCOMUtils
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
// globals Services
Cu.import("resource://gre/modules/Services.jsm");

function log(str) {
  // dump("-*- PresentationRequestUIGlue.js -*-: " + str + "\n");
}

function PresentationRequestUIGlue() { }

PresentationRequestUIGlue.prototype = {
  sendRequest: function sendRequest(aURL, aSessionId, aDevice) {
    log("PresentationRequestUIGlue - sendRequest aURL=" + aURL +
        " aSessionId=" + aSessionId);

    let localDevice;
    try {
      localDevice = aDevice.QueryInterface(Ci.nsIPresentationLocalDevice);
    } catch (e) {
      /* XXX: Currently, Fennec only support 1-UA devices. Remove this
       * Promise.reject() when it starts to support 2-UA devices.
       */
      log("Not an 1-UA device.");
      return new Promise.reject();
    }

    return new Promise((aResolve, aReject) => {

      let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                            .getService(Ci.nsIUUIDGenerator);
      let requestId = uuidGenerator.generateUUID().toString();

      let handleObserve = (aSubject, aTopic, aData) => {
        log("Got observe: aTopic=" + aTopic);

        let data = JSON.parse(aData);
        if (data.requestId != requestId) {
          return;
        }

        Services.obs.removeObserver(handleObserve,
                                    TOPIC_PRESENTATION_RECEIVER_LAUNCH_RESPONSE);
        switch (data.result) {
          case "success":
            aResolve(aSubject);
            break;
          case "error":
            aReject();
            break;
        }
      };

      Services.obs.addObserver(handleObserve,
                               TOPIC_PRESENTATION_RECEIVER_LAUNCH_RESPONSE);

      let data = {
        url: aURL,
        windowId: localDevice.windowId,
        requestId: requestId
      };
      Services.obs.notifyObservers(null,
                                   TOPIC_PRESENTATION_RECEIVER_LAUNCH,
                                   JSON.stringify(data));
    });
  },

  classID: Components.ID("9c550ef7-3ff6-4bd1-9ad1-5a3735b90d21"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationRequestUIGlue])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PresentationRequestUIGlue]);
