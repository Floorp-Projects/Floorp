/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Notifications", "resource://gre/modules/Notifications.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "contentPrefs",
                                   "@mozilla.org/content-pref/service;1",
                                   "nsIContentPrefService2");

var WebrtcUI = {
  _notificationId: null,
  VIDEO_SOURCE: "videoSource",
  AUDIO_SOURCE: "audioDevice",

  observe: function(aSubject, aTopic, aData) {
    if (aTopic === "getUserMedia:request") {
      this.handleRequest(aSubject, aTopic, aData);
    } else if (aTopic === "recording-device-events") {
      switch (aData) {
        case "shutdown":
        case "starting":
          this.notify();
          break;
      }
    }
  },

  notify: function() {
    let windows = MediaManagerService.activeMediaCaptureWindows;
    let count = windows.Count();
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
        ongoing: true
      };

      let cameraActive = false;
      let audioActive = false;
      for (let i = 0; i < count; i++) {
        let win = windows.GetElementAt(i);
        let hasAudio = {};
        let hasVideo = {};
        MediaManagerService.mediaCaptureWindowState(win, hasVideo, hasAudio);
        if (hasVideo.value) cameraActive = true;
        if (hasAudio.value) audioActive = true;
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
        throw "Couldn't find any cameras or microphones being used"
      }

      if (this._notificationId)
          Notifications.update(this._notificationId, notificationOptions);
      else
        this._notificationId = Notifications.create(notificationOptions);
      if (count > 1)
        msg.count = count;
    }
  },

  handleRequest: function handleRequest(aSubject, aTopic, aData) {
    let constraints = aSubject.getConstraints();
    let contentWindow = Services.wm.getOuterWindowWithId(aSubject.windowID);

    contentWindow.navigator.mozGetUserMediaDevices(
      constraints,
      function (aDevices) {
        WebrtcUI.prompt(contentWindow, aSubject.callID, constraints.audio, constraints.video, aDevices);
      },
      Cu.reportError, aSubject.innerWindowID);
  },

  getDeviceButtons: function(aAudioDevices, aVideoDevices, aCallID, aHost) {
    return [{
      label: Strings.browser.GetStringFromName("getUserMedia.denyRequest.label"),
      callback: () => {
        Services.obs.notifyObservers(null, "getUserMedia:response:deny", aCallID);
      }
    }, {
      label: Strings.browser.GetStringFromName("getUserMedia.shareRequest.label"),
      callback: (checked /* ignored */, inputs) => {
        let allowedDevices = Cc["@mozilla.org/supports-array;1"].createInstance(Ci.nsISupportsArray);

        let audioId = 0;
        if (inputs && inputs[this.AUDIO_SOURCE] != undefined) {
          audioId = inputs[this.AUDIO_SOURCE];
        }

        if (aAudioDevices[audioId]) {
          allowedDevices.AppendElement(aAudioDevices[audioId]);
          this.setDefaultDevice(this.AUDIO_SOURCE, aAudioDevices[audioId].name, aHost);
        }

        let videoId = 0;
        if (inputs && inputs[this.VIDEO_SOURCE] != undefined) {
          videoId = inputs[this.VIDEO_SOURCE];
        }

        if (aVideoDevices[videoId]) {
          allowedDevices.AppendElement(aVideoDevices[videoId]);
          this.setDefaultDevice(this.VIDEO_SOURCE, aVideoDevices[videoId].name, aHost);
        }

        Services.obs.notifyObservers(allowedDevices, "getUserMedia:response:allow", aCallID);
      }
    }];
  },

  // Get a list of string names for devices. Ensures that none of the strings are blank
  _getList: function(aDevices, aType) {
    let defaultCount = 0;
    return aDevices.map(function(device) {
      let name = device.name;
      // if this is a Camera input, convert the name to something readable
      let res = /Camera\ \d+,\ Facing (front|back)/.exec(name);
      if (res) {
        return Strings.browser.GetStringFromName("getUserMedia." + aType + "." + res[1] + "Camera");
      }

      if (name.startsWith("&") && name.endsWith(";")) {
        return Strings.browser.GetStringFromName(name.substring(1, name.length -1));
      }

      if (name.trim() == "") {
        defaultCount++;
        return Strings.browser.formatStringFromName("getUserMedia." + aType + ".default", [defaultCount], 1);
      }

      return name;
    }, this);
  },

  _addDevicesToOptions: function(aDevices, aType, aOptions, aHost, aContext) {
    if (aDevices.length == 0) {
      return Promise.resolve(aOptions);
    }

    let updateOptions = () => {
      // Filter out empty items from the list
      let list = this._getList(aDevices, aType);
      if (list.length > 0) {
        aOptions.inputs.push({
          id: aType,
          type: "menulist",
          label: Strings.browser.GetStringFromName("getUserMedia." + aType + ".prompt"),
          values: list
        });
      }

      return aOptions;
    }

    return this.getDefaultDevice(aType, aHost, aContext).then((defaultDevice) => {
      aDevices.sort((a, b) => {
        if (b.name === defaultDevice) return 1;
        return 0;
      });
      return updateOptions();
    }).catch(updateOptions);
  },

  // Sets the default for a aHost. If no aHost is specified, sets the browser wide default.
  // Saving is async, but this doesn't wait for a result.
  setDefaultDevice: function(aType, aValue, aHost, aContext) {
    if (aHost) {
      contentPrefs.set(aHost, "webrtc." + aType, aValue, aContext);
    } else {
      contentPrefs.setGlobal("webrtc." + aType, aValue, aContext);
    }
  },

  _checkContentPref(aHost, aType, aContext) {
    return new Promise((resolve, reject) => {
      let result = null;
      let handler = {
        handleResult: (aResult) => result = aResult,
        handleCompletion: function(aReason) {
          if (aReason == Components.interfaces.nsIContentPrefCallback2.COMPLETE_OK &&
              result instanceof Components.interfaces.nsIContentPref) {
            resolve(result.value);
          } else {
            reject(result);
          }
        }
      };

      if (aHost) {
        contentPrefs.getByDomainAndName(aHost, "webrtc." + aType, aContext, handler);
      } else {
        contentPrefs.getGlobal("webrtc." + aType, aContext, handler);
      }
    });
  },

  // Returns the default device for this aHost. If no aHost is specified, returns a browser wide default
  getDefaultDevice: function(aType, aHost, aContext) {
    return this._checkContentPref(aHost, aType, aContext).catch(() => {
      // If we found nothing for the initial pref, try looking for a global one
      return this._checkContentPref(null, aType, aContext);
    });
  },

  prompt: function (aWindow, aCallID, aAudioRequested, aVideoRequested, aDevices) {
    let audioDevices = [];
    let videoDevices = [];

    // Split up all the available aDevices into audio and video categories
    for (let device of aDevices) {
      device = device.QueryInterface(Ci.nsIMediaDevice);
      switch (device.type) {
      case "audio":
        if (aAudioRequested) {
          audioDevices.push(device);
        }
        break;
      case "video":
        if (aVideoRequested) {
          videoDevices.push(device);
        }
        break;
      }
    }

    // Bsaed on the aTypes available, setup the prompt and icon text
    let requestType;
    if (audioDevices.length && videoDevices.length) {
      requestType = "CameraAndMicrophone";
    } else if (audioDevices.length) {
      requestType = "Microphone";
    } else if (videoDevices.length) {
      requestType = "Camera";
    } else {
      return;
    }

    let host = aWindow.document.documentURIObject.host;
    // Show the app name if this is a WebRT app, otherwise show the host.
    let requestor = BrowserApp.manifest ? "'" + BrowserApp.manifest.name  + "'" : host;
    let message = Strings.browser.formatStringFromName("getUserMedia.share" + requestType + ".message", [ requestor ], 1);

    let options = { inputs: [] };
    // if the users only option would be to select "No Audio" or "No Video"
    // i.e. we're only showing audio or only video and there is only one device for that type
    // don't bother showing a menulist to select from
    if (videoDevices.length > 0 && audioDevices.length > 0) {
      videoDevices.push({ name: Strings.browser.GetStringFromName("getUserMedia.videoSource.none") });
      audioDevices.push({ name: Strings.browser.GetStringFromName("getUserMedia.audioDevice.none") });
    }

    let loadContext = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIWebNavigation)
                             .QueryInterface(Ci.nsILoadContext);
    // videoSource is both the string used for l10n lookup and the object that will be returned
    this._addDevicesToOptions(videoDevices, this.VIDEO_SOURCE, options, host, loadContext).then((aOptions) => {
      return this._addDevicesToOptions(audioDevices, this.AUDIO_SOURCE, aOptions, host, loadContext);
    }).catch(Cu.reportError).then((aOptions) => {
      let buttons = this.getDeviceButtons(audioDevices, videoDevices, aCallID, host);
      NativeWindow.doorhanger.show(message, "webrtc-request", buttons, BrowserApp.selectedTab.id, aOptions);
    });
  }
}
