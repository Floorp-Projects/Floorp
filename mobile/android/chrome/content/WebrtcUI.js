/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var WebrtcUI = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "getUserMedia:request") {
      this.handleRequest(aSubject, aTopic, aData);
    }
  },

  handleRequest: function handleRequest(aSubject, aTopic, aData) {
    let { windowID: windowID, callID: callID } = JSON.parse(aData);

    let someWindow = Services.wm.getMostRecentWindow("navigator:browser");
    if (someWindow != window)
      return;

    let contentWindow = someWindow.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDOMWindowUtils).getOuterWindowWithId(windowID);
    let browser = BrowserApp.getBrowserForWindow(contentWindow);
    let params = aSubject.QueryInterface(Ci.nsIMediaStreamOptions);

    browser.ownerDocument.defaultView.navigator.mozGetUserMediaDevices(
      function (devices) {
        WebrtcUI.prompt(browser, callID, params.audio, params.video, devices);
      },
      function (error) {
        Cu.reportError(error);
      }
    );
  },

  getDeviceButtons: function(aDevices, aCallID, stringBundle) {
    let buttons = [{
      label: stringBundle.GetStringFromName("getUserMedia.denyRequest.label"),
      callback: function() {
        Services.obs.notifyObservers(null, "getUserMedia:response:deny", aCallID);
      }
    }];
    for (let device of aDevices) {
      let button = {
        label: stringBundle.GetStringFromName("getUserMedia.shareRequest.label"),
        callback: function() {
          let allowedDevices = Cc["@mozilla.org/supports-array;1"].createInstance(Ci.nsISupportsArray);
          allowedDevices.AppendElement(device);
          Services.obs.notifyObservers(allowedDevices, "getUserMedia:response:allow", aCallID);
          // Show browser-specific indicator for the active camera/mic access.
          // XXX: Mobile?
        }
      };
      buttons.push(button);
    }

    return buttons;
  },

  prompt: function prompt(aBrowser, aCallID, aAudioRequested, aVideoRequested, aDevices) {
    let audioDevices = [];
    let videoDevices = [];
    for (let device of aDevices) {
      device = device.QueryInterface(Ci.nsIMediaDevice);
      switch (device.type) {
      case "audio":
        if (aAudioRequested)
          audioDevices.push(device);
        break;
      case "video":
        if (aVideoRequested)
          videoDevices.push(device);
        break;
      }
    }

    let requestType;
    if (audioDevices.length && videoDevices.length)
      requestType = "CameraAndMicrophone";
    else if (audioDevices.length)
      requestType = "Microphone";
    else if (videoDevices.length)
      requestType = "Camera";
    else
      return;

    let host = aBrowser.contentDocument.documentURIObject.asciiHost;
    let stringBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    let message = stringBundle.formatStringFromName("getUserMedia.share" + requestType + ".message", [ host ], 1);

    if (audioDevices.length) {
      let buttons = this.getDeviceButtons(audioDevices, aCallID, stringBundle);
      NativeWindow.doorhanger.show(message, "webrtc-request-audio", buttons, BrowserApp.selectedTab.id);
    }

    if (videoDevices.length) {
      let buttons = this.getDeviceButtons(videoDevices, aCallID, stringBundle);
      NativeWindow.doorhanger.show(message, "webrtc-request-video", buttons, BrowserApp.selectedTab.id);
    }
  }
}
