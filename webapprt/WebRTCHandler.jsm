/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [];

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function handleRequest(aSubject, aTopic, aData) {
  let { windowID, callID } = aSubject;
  let constraints = aSubject.getConstraints();
  let contentWindow = Services.wm.getOuterWindowWithId(windowID);

  contentWindow.navigator.mozGetUserMediaDevices(
    constraints,
    function (devices) {
      prompt(contentWindow, callID, constraints.audio,
             constraints.video || constraints.picture,
             devices);
    },
    function (error) {
      denyRequest(callID, error);
    });
}

function prompt(aWindow, aCallID, aAudioRequested, aVideoRequested, aDevices) {
  let audioDevices = [];
  let videoDevices = [];
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

  if (audioDevices.length == 0 && videoDevices.length == 0) {
    denyRequest(aCallID);
    return;
  }

  let params = {
                 videoDevices: videoDevices,
                 audioDevices: audioDevices,
                 out: null
               };
  aWindow.openDialog("chrome://webapprt/content/getUserMediaDialog.xul", "",
                     "chrome, dialog, modal", params).focus();

  if (!params.out) {
    denyRequest(aCallID);
    return;
  }

  let allowedDevices = Cc["@mozilla.org/supports-array;1"].
                       createInstance(Ci.nsISupportsArray);
  let videoIndex = params.out.video;
  let audioIndex = params.out.audio;

  if (videoIndex != -1) {
    allowedDevices.AppendElement(videoDevices[videoIndex]);
  }

  if (audioIndex != -1) {
    allowedDevices.AppendElement(audioDevices[audioIndex]);
  }

  if (allowedDevices.Count()) {
    Services.obs.notifyObservers(allowedDevices, "getUserMedia:response:allow",
                                 aCallID);
  } else {
    denyRequest(aCallID);
  }
}

function denyRequest(aCallID, aError) {
  let msg = null;
  if (aError) {
    msg = Cc["@mozilla.org/supports-string;1"].
          createInstance(Ci.nsISupportsString);
    msg.data = aError;
  }

  Services.obs.notifyObservers(msg, "getUserMedia:response:deny", aCallID);
}

Services.obs.addObserver(handleRequest, "getUserMedia:request", false);
