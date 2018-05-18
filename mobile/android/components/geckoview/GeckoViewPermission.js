/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

// See: http://developer.android.com/reference/android/Manifest.permission.html
const PERM_ACCESS_COARSE_LOCATION = "android.permission.ACCESS_COARSE_LOCATION";
const PERM_ACCESS_FINE_LOCATION = "android.permission.ACCESS_FINE_LOCATION";
const PERM_CAMERA = "android.permission.CAMERA";
const PERM_RECORD_AUDIO = "android.permission.RECORD_AUDIO";

function GeckoViewPermission() {
  this.wrappedJSObject = this;
}

GeckoViewPermission.prototype = {
  classID: Components.ID("{42f3c238-e8e8-4015-9ca2-148723a8afcf}"),

  QueryInterface: ChromeUtils.generateQI([
      Ci.nsIObserver, Ci.nsIContentPermissionPrompt]),

  _appPermissions: {},

  /* ----------  nsIObserver  ---------- */
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "getUserMedia:ask-device-permission": {
        this.handleMediaAskDevicePermission(aData, aSubject);
        break;
      }
      case "getUserMedia:request": {
        this.handleMediaRequest(aSubject);
        break;
      }
      case "PeerConnection:request": {
        this.handlePeerConnectionRequest(aSubject);
        break;
      }
    }
  },

  receiveMessage(aMsg) {
    switch (aMsg.name) {
      case "GeckoView:AddCameraPermission": {
        let uri;
        try {
          // This fails for principals that serialize to "null", e.g. file URIs.
          uri = Services.io.newURI(aMsg.data.origin);
        } catch (e) {
          uri = Services.io.newURI(aMsg.data.documentURI);
        }
        // Although the lifetime is "session" it will be removed upon
        // use so it's more of a one-shot.
        Services.perms.add(uri, "MediaManagerVideo",
                           Services.perms.ALLOW_ACTION,
                           Services.perms.EXPIRE_SESSION);
        break;
      }
    }
  },

  handleMediaAskDevicePermission: function(aType, aCallback) {
    let perms = [];
    if (aType === "video" || aType === "all") {
      perms.push(PERM_CAMERA);
    }
    if (aType === "audio" || aType === "all") {
      perms.push(PERM_RECORD_AUDIO);
    }

    let [dispatcher] = GeckoViewUtils.getActiveDispatcherAndWindow();
    let callback = _ => {
      Services.obs.notifyObservers(aCallback, "getUserMedia:got-device-permission");
    };

    if (dispatcher) {
      this.getAppPermissions(dispatcher, perms).then(callback, callback);
    } else {
      // No dispatcher; just bail.
      callback();
    }
  },

  handleMediaRequest: function(aRequest) {
    let constraints = aRequest.getConstraints();
    let callId = aRequest.callID;
    let denyRequest = _ => {
      Services.obs.notifyObservers(null, "getUserMedia:response:deny", callId);
    };

    let win = Services.wm.getOuterWindowWithId(aRequest.windowID);
    new Promise((resolve, reject) => {
      win.navigator.mozGetUserMediaDevices(constraints, resolve, reject,
                                           aRequest.innerWindowID, callId);
      // Release the request first.
      aRequest = undefined;
    }).then(devices => {
      if (win.closed) {
        return;
      }

      let sources = devices.map(device => {
        device = device.QueryInterface(Ci.nsIMediaDevice);
        return {
          type: device.type,
          id: device.id,
          rawId: device.rawId,
          name: device.name,
          mediaSource: device.mediaSource,
        };
      });

      if (constraints.video && !sources.some(source => source.type === "video")) {
        throw "no video source";
      } else if (constraints.audio && !sources.some(source => source.type === "audio")) {
        throw "no audio source";
      }

      let dispatcher = GeckoViewUtils.getDispatcherForWindow(win);
      let uri = win.document.documentURIObject;
      return dispatcher.sendRequestForResult({
        type: "GeckoView:MediaPermission",
        uri: uri.displaySpec,
        video: constraints.video ? sources.filter(source => source.type === "video") : null,
        audio: constraints.audio ? sources.filter(source => source.type === "audio") : null,
      }).then(response => {
        if (!response) {
          // Rejected.
          denyRequest();
          return;
        }
        let allowedDevices = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
        if (constraints.video) {
          let video = devices.find(device => response.video === device.id);
          if (!video) {
            throw new Error("invalid video id");
          }
          Services.cpmm.sendAsyncMessage("GeckoView:AddCameraPermission", {
            origin: win.origin,
            documentURI: win.document.documentURI,
          });
          allowedDevices.appendElement(video);
        }
        if (constraints.audio) {
          let audio = devices.find(device => response.audio === device.id);
          if (!audio) {
            throw new Error("invalid audio id");
          }
          allowedDevices.appendElement(audio);
        }
        Services.obs.notifyObservers(
            allowedDevices, "getUserMedia:response:allow", callId);
      });
    }).catch(error => {
      Cu.reportError("Media device error: " + error);
      denyRequest();
    });
  },

  handlePeerConnectionRequest: function(aRequest) {
    Services.obs.notifyObservers(null, "PeerConnection:response:allow", aRequest.callID);
  },

  checkAppPermissions: function(aPerms) {
    return aPerms.every(perm => this._appPermissions[perm]);
  },

  getAppPermissions: function(aDispatcher, aPerms) {
    let perms = aPerms.filter(perm => !this._appPermissions[perm]);
    if (!perms.length) {
      return Promise.resolve(/* granted */ true);
    }
    return aDispatcher.sendRequestForResult({
      type: "GeckoView:AndroidPermission",
      perms: perms,
    }).then(granted => {
      if (granted) {
        for (let perm of perms) {
          this._appPermissions[perm] = true;
        }
      }
      return granted;
    });
  },

  prompt: function(aRequest) {
    // Only allow exactly one permission request here.
    let types = aRequest.types.QueryInterface(Ci.nsIArray);
    if (types.length !== 1) {
      aRequest.cancel();
      return;
    }

    let perm = types.queryElementAt(0, Ci.nsIContentPermissionType);
    let dispatcher = GeckoViewUtils.getDispatcherForWindow(
        aRequest.window ? aRequest.window : aRequest.element.ownerGlobal);
    dispatcher.sendRequestForResult({
        type: "GeckoView:ContentPermission",
        uri: aRequest.principal.URI.displaySpec,
        perm: perm.type,
        access: perm.access !== "unused" ? perm.access : null,
    }).then(granted => {
      if (!granted) {
        return false;
      }
      // Ask for app permission after asking for content permission.
      if (perm.type === "geolocation") {
        return this.getAppPermissions(dispatcher, [PERM_ACCESS_FINE_LOCATION]);
      }
      return true;
    }).catch(error => {
      Cu.reportError("Permission error: " + error);
      return /* granted */ false;
    }).then(granted => {
      (granted ? aRequest.allow : aRequest.cancel)();
      // Manually release the target request here to facilitate garbage collection.
      aRequest = undefined;
    });
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([GeckoViewPermission]);
