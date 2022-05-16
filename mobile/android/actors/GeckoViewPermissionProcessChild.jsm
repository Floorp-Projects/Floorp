/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPermissionProcessChild"];

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

class GeckoViewPermissionProcessChild extends JSProcessActorChild {
  /* ----------  nsIObserver  ---------- */
  async observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "getUserMedia:ask-device-permission": {
        await this.sendQuery("AskDevicePermission", aData);
        Services.obs.notifyObservers(
          aSubject,
          "getUserMedia:got-device-permission"
        );
        break;
      }
      case "getUserMedia:request": {
        const { callID } = aSubject;
        const allowedDevices = await this.handleMediaRequest(aSubject);
        Services.obs.notifyObservers(
          allowedDevices,
          allowedDevices
            ? "getUserMedia:response:allow"
            : "getUserMedia:response:deny",
          callID
        );
        break;
      }
      case "PeerConnection:request": {
        Services.obs.notifyObservers(
          null,
          "PeerConnection:response:allow",
          aSubject.callID
        );
        break;
      }
    }
  }

  async handleMediaRequest(aRequest) {
    const constraints = aRequest.getConstraints();
    const { devices, windowID } = aRequest;
    const window = Services.wm.getOuterWindowWithId(windowID);
    if (window.closed) {
      return null;
    }

    // Release the request first
    aRequest = undefined;

    const sources = devices.map(device => {
      device = device.QueryInterface(Ci.nsIMediaDevice);
      return {
        type: device.type,
        id: device.rawId,
        rawId: device.rawId,
        name: device.rawName, // unfiltered device name to show to the user
        mediaSource: device.mediaSource,
      };
    });

    if (
      constraints.video &&
      !sources.some(source => source.type === "videoinput")
    ) {
      Cu.reportError("Media device error: no video source");
      return null;
    } else if (
      constraints.audio &&
      !sources.some(source => source.type === "audioinput")
    ) {
      Cu.reportError("Media device error: no audio source");
      return null;
    }

    const response = await window.windowGlobalChild
      .getActor("GeckoViewPermission")
      .getMediaPermission({
        uri: window.document.documentURI,
        video: constraints.video
          ? sources.filter(source => source.type === "videoinput")
          : null,
        audio: constraints.audio
          ? sources.filter(source => source.type === "audioinput")
          : null,
      });

    if (!response) {
      // Rejected.
      return null;
    }

    const allowedDevices = Cc["@mozilla.org/array;1"].createInstance(
      Ci.nsIMutableArray
    );
    if (constraints.video) {
      const video = devices.find(device => response.video === device.rawId);
      if (!video) {
        Cu.reportError("Media device error: invalid video id");
        return null;
      }
      await window.windowGlobalChild
        .getActor("GeckoViewPermission")
        .addCameraPermission();
      allowedDevices.appendElement(video);
    }
    if (constraints.audio) {
      const audio = devices.find(device => response.audio === device.rawId);
      if (!audio) {
        Cu.reportError("Media device error: invalid audio id");
        return null;
      }
      allowedDevices.appendElement(audio);
    }
    return allowedDevices;
  }
}

const { debug, warn } = GeckoViewUtils.initLogging(
  "GeckoViewPermissionProcessChild"
);
