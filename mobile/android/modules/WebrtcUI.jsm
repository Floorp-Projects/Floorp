/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["WebrtcUI"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  DoorHanger: "resource://gre/modules/Prompt.jsm",
  Notifications: "resource://gre/modules/Notifications.jsm",
  RuntimePermissions: "resource://gre/modules/RuntimePermissions.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyServiceGetter(this, "MediaManagerService",
                                   "@mozilla.org/mediaManagerService;1",
                                   "nsIMediaManagerService");
XPCOMUtils.defineLazyServiceGetter(this, "ParentalControls",
                                   "@mozilla.org/parental-controls-service;1",
                                   "nsIParentalControlsService");

var Strings = {};

XPCOMUtils.defineLazyGetter(Strings, "brand", _ =>
        Services.strings.createBundle("chrome://branding/locale/brand.properties"));
XPCOMUtils.defineLazyGetter(Strings, "browser", _ =>
        Services.strings.createBundle("chrome://browser/locale/browser.properties"));

var WebrtcUI = {
  _notificationId: null,

  // Add-ons can override stock permission behavior by doing:
  //
  //   var stockObserve = WebrtcUI.observe;
  //
  //   webrtcUI.observe = function(aSubject, aTopic, aData) {
  //     switch (aTopic) {
  //      case "PeerConnection:request": {
  //        // new code.
  //        break;
  //      ...
  //      default:
  //        return stockObserve.call(this, aSubject, aTopic, aData);
  //
  // See browser/modules/webrtcUI.jsm for details.

  observe: function(aSubject, aTopic, aData) {
    if (aTopic === "getUserMedia:ask-device-permission") {
      RuntimePermissions
        .waitForPermissions(this._determineNeededRuntimePermissions(aData))
        .then(_ => {
          Services.obs.notifyObservers(aSubject, "getUserMedia:got-device-permission");
        });
    } else if (aTopic === "getUserMedia:request") {
      RuntimePermissions
        .checkPermissions(this._determineNeededRuntimePermissions(aSubject))
        .then((permissionGranted) => {
          if (permissionGranted) {
            WebrtcUI.handleGumRequest(aSubject, aTopic, aData);
          } else {
            Services.obs.notifyObservers(null, "getUserMedia:response:deny", aSubject.callID);
          }
});
    } else if (aTopic === "PeerConnection:request") {
      this.handlePCRequest(aSubject, aTopic, aData);
    } else if (aTopic === "recording-device-events") {
      this.notify();
    } else if (aTopic === "VideoCapture:Paused") {
      if (this._notificationId) {
        Notifications.cancel(this._notificationId);
        this._notificationId = null;
      }
    } else if (aTopic === "VideoCapture:Resumed") {
      this.notify();
    }
  },

  notify: function() {
    let windows = MediaManagerService.activeMediaCaptureWindows;
    let count = windows.length;
    let msg = {};
    if (count == 0) {
      if (this._notificationId) {
        Notifications.cancel(this._notificationId);
        this._notificationId = null;
      }
    } else {
      let notificationOptions = {
        title: Strings.brand.GetStringFromName("brandShortName"),
        when: null, // hide the date row
        light: [0xFF9500FF, 1000, 1000],
        ongoing: true,
      };

      let cameraActive = false;
      let audioActive = false;
      for (let i = 0; i < count; i++) {
        let win = windows.queryElementAt(i, Ci.nsIDOMWindow);
        let hasCamera = {};
        let hasMicrophone = {};
        MediaManagerService.mediaCaptureWindowState(win, hasCamera, hasMicrophone);
        if (hasCamera.value != MediaManagerService.STATE_NOCAPTURE) cameraActive = true;
        if (hasMicrophone.value != MediaManagerService.STATE_NOCAPTURE) audioActive = true;
      }

      if (cameraActive && audioActive) {
        notificationOptions.message = Strings.browser.GetStringFromName("getUserMedia.sharingCameraAndMicrophone.message2");
        notificationOptions.icon = "drawable:alert_mic_camera";
      } else if (cameraActive) {
        notificationOptions.message = Strings.browser.GetStringFromName("getUserMedia.sharingCamera.message2");
        notificationOptions.icon = "drawable:alert_camera";
      } else if (audioActive) {
        notificationOptions.message = Strings.browser.GetStringFromName("getUserMedia.sharingMicrophone.message2");
        notificationOptions.icon = "drawable:alert_mic";
      } else {
        // somethings wrong. lets throw
        throw new Error("Couldn't find any cameras or microphones being used");
      }

      if (this._notificationId)
          Notifications.update(this._notificationId, notificationOptions);
      else
        this._notificationId = Notifications.create(notificationOptions);
      if (count > 1)
        msg.count = count;
    }
  },

  handlePCRequest: function handlePCRequest(aSubject, aTopic, aData) {
    aSubject = aSubject.wrappedJSObject;
    let { callID } = aSubject;
    // Also available: windowID, isSecure, innerWindowID. For contentWindow do:
    //
    //   let contentWindow = Services.wm.getOuterWindowWithId(windowID);

    Services.obs.notifyObservers(null, "PeerConnection:response:allow", callID);
  },

  handleGumRequest: function handleGumRequest(aSubject, aTopic, aData) {
    let constraints = aSubject.getConstraints();
    let contentWindow = Services.wm.getOuterWindowWithId(aSubject.windowID);

    contentWindow.navigator.mozGetUserMediaDevices(
      constraints,
      function(devices) {
        if (!ParentalControls.isAllowed(ParentalControls.CAMERA_MICROPHONE)) {
          Services.obs.notifyObservers(null, "getUserMedia:response:deny", aSubject.callID);
          WebrtcUI.showBlockMessage(contentWindow, devices);
          return;
        }

        WebrtcUI.prompt(contentWindow, aSubject.callID, constraints.audio,
                        constraints.video, devices);
      },
      function(error) {
        Cu.reportError(error);
      },
      aSubject.innerWindowID,
      aSubject.callID);
  },

  getDeviceButtons: function(audioDevices, videoDevices, aCallID, aUri) {
    return [{
      label: Strings.browser.GetStringFromName("getUserMedia.denyRequest.label"),
      callback: function() {
        Services.obs.notifyObservers(null, "getUserMedia:response:deny", aCallID);
      },
    },
    {
      label: Strings.browser.GetStringFromName("getUserMedia.shareRequest.label"),
      callback: function(checked /* ignored */, inputs) {
        let allowedDevices = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

        let audioId = 0;
        if (inputs && inputs.audioDevice != undefined)
          audioId = inputs.audioDevice;
        if (audioDevices[audioId])
          allowedDevices.appendElement(audioDevices[audioId]);

        let videoId = 0;
        if (inputs && inputs.videoSource != undefined)
          videoId = inputs.videoSource;
        if (videoDevices[videoId]) {
          allowedDevices.appendElement(videoDevices[videoId]);
          let perms = Services.perms;
          // Although the lifetime is "session" it will be removed upon
          // use so it's more of a one-shot.
          perms.add(aUri, "MediaManagerVideo", perms.ALLOW_ACTION, perms.EXPIRE_SESSION);
        }

        Services.obs.notifyObservers(allowedDevices, "getUserMedia:response:allow", aCallID);
      },
      positive: true,
    }];
  },

  _determineNeededRuntimePermissions: function(aSubject) {
    let permissions = [];

    let constraints;
    if (typeof aSubject === "string") {
      constraints = {
        video: aSubject === "video" || aSubject === "all",
        audio: aSubject === "audio" || aSubject === "all",
      };
    } else {
      constraints = aSubject.getConstraints();
    }

    if (constraints.video) {
      permissions.push(RuntimePermissions.CAMERA);
    }
    if (constraints.audio) {
      permissions.push(RuntimePermissions.RECORD_AUDIO);
    }

    return permissions;
  },

  // Get a list of string names for devices. Ensures that none of the strings are blank
  _getList: function(aDevices, aType) {
    let defaultCount = 0;
    return aDevices.map(function(device) {
        // if this is a Camera input, convert the name to something readable
        let res = /Camera\ \d+,\ Facing (front|back)/.exec(device.name);
        if (res)
          return Strings.browser.GetStringFromName("getUserMedia." + aType + "." + res[1] + "Camera");

        if (device.name.startsWith("&") && device.name.endsWith(";"))
          return Strings.browser.GetStringFromName(device.name.substring(1, device.name.length - 1));

        if (device.name.trim() == "") {
          defaultCount++;
          return Strings.browser.formatStringFromName("getUserMedia." + aType + ".default", [defaultCount], 1);
        }
        return device.name;
      }, this);
  },

  _addDevicesToOptions: function(aDevices, aType, aOptions) {
    if (aDevices.length) {
      // Filter out empty items from the list
      let list = this._getList(aDevices, aType);

      if (list.length > 0) {
        aOptions.inputs.push({
          id: aType,
          type: "menulist",
          label: Strings.browser.GetStringFromName("getUserMedia." + aType + ".prompt"),
          values: list,
        });
      }
    }
  },

  showBlockMessage: function(aWindow, aDevices) {
    let microphone = false;
    let camera = false;

    for (let device of aDevices) {
      device = device.QueryInterface(Ci.nsIMediaDevice);
      if (device.type == "audioinput") {
        microphone = true;
      } else if (device.type == "videoinput") {
        camera = true;
      }
    }

    let message;
    if (microphone && !camera) {
      message = Strings.browser.GetStringFromName("getUserMedia.blockedMicrophoneAccess");
    } else if (camera && !microphone) {
      message = Strings.browser.GetStringFromName("getUserMedia.blockedCameraAccess");
    } else {
      message = Strings.browser.GetStringFromName("getUserMedia.blockedCameraAndMicrophoneAccess");
    }

    DoorHanger.show(aWindow, message, "webrtc-blocked");
  },

  getChromeWindow: function getChromeWindow(aWindow) {
     let chromeWin = aWindow.docShell.rootTreeItem.domWindow
                            .QueryInterface(Ci.nsIDOMChromeWindow);
     return chromeWin;
  },

  prompt: function prompt(aContentWindow, aCallID, aAudioRequested,
                          aVideoRequested, aDevices) {
    let audioDevices = [];
    let videoDevices = [];
    for (let device of aDevices) {
      device = device.QueryInterface(Ci.nsIMediaDevice);
      switch (device.type) {
      case "audioinput":
        if (aAudioRequested)
          audioDevices.push(device);
        break;
      case "videoinput":
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

    let chromeWin = this.getChromeWindow(aContentWindow);
    let uri = aContentWindow.document.documentURIObject;
    let host = uri.host;
    let requestor = (chromeWin.BrowserApp && chromeWin.BrowserApp.manifest) ?
          "'" + chromeWin.BrowserApp.manifest.name + "'" : host;
    let message = Strings.browser.formatStringFromName("getUserMedia.share" + requestType + ".message", [ requestor ], 1);

    let options = { inputs: [] };
    if (videoDevices.length > 1 || audioDevices.length > 0) {
      // videoSource is both the string used for l10n lookup and the object that will be returned
      this._addDevicesToOptions(videoDevices, "videoSource", options);
    }

    if (audioDevices.length > 1 || videoDevices.length > 0) {
      this._addDevicesToOptions(audioDevices, "audioDevice", options);
    }

    let buttons = this.getDeviceButtons(audioDevices, videoDevices, aCallID, uri);

    DoorHanger.show(aContentWindow, message, "webrtc-request", buttons, options, "WEBRTC");
  },
};
