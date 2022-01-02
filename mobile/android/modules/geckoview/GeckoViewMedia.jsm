/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewRecordingMedia"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "MediaManagerService",
  "@mozilla.org/mediaManagerService;1",
  "nsIMediaManagerService"
);

const STATUS_RECORDING = "recording";
const STATUS_INACTIVE = "inactive";
const TYPE_CAMERA = "camera";
const TYPE_MICROPHONE = "microphone";

const GeckoViewRecordingMedia = {
  // The event listener for this is hooked up in GeckoViewStartup.jsm
  observe(aSubject, aTopic, aData) {
    debug`observe: aTopic=${aTopic}`;
    switch (aTopic) {
      case "recording-device-events": {
        this.handleRecordingDeviceEvents();
        break;
      }
    }
  },

  handleRecordingDeviceEvents() {
    const [dispatcher] = GeckoViewUtils.getActiveDispatcherAndWindow();
    if (dispatcher) {
      const windows = MediaManagerService.activeMediaCaptureWindows;
      const devices = [];

      const getStatusString = function(activityStatus) {
        switch (activityStatus) {
          case MediaManagerService.STATE_CAPTURE_ENABLED:
          case MediaManagerService.STATE_CAPTURE_DISABLED:
            return STATUS_RECORDING;
          case MediaManagerService.STATE_NOCAPTURE:
            return STATUS_INACTIVE;
          default:
            throw new Error("Unexpected activityStatus value");
        }
      };

      for (let i = 0; i < windows.length; i++) {
        const win = windows.queryElementAt(i, Ci.nsIDOMWindow);
        const hasCamera = {};
        const hasMicrophone = {};
        const screen = {};
        const window = {};
        const browser = {};
        const mediaDevices = {};
        MediaManagerService.mediaCaptureWindowState(
          win,
          hasCamera,
          hasMicrophone,
          screen,
          window,
          browser,
          mediaDevices
        );
        var cameraStatus = getStatusString(hasCamera.value);
        var microphoneStatus = getStatusString(hasMicrophone.value);
        if (hasCamera.value != MediaManagerService.STATE_NOCAPTURE) {
          devices.push({
            type: TYPE_CAMERA,
            status: cameraStatus,
          });
        }
        if (hasMicrophone.value != MediaManagerService.STATE_NOCAPTURE) {
          devices.push({
            type: TYPE_MICROPHONE,
            status: microphoneStatus,
          });
        }
      }
      dispatcher.sendRequestForResult({
        type: "GeckoView:MediaRecordingStatusChanged",
        devices,
      });
    } else {
      console.log("no dispatcher present");
    }
  },
};

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewMedia");
