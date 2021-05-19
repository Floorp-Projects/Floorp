/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPermission"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

// See: http://developer.android.com/reference/android/Manifest.permission.html
const PERM_ACCESS_FINE_LOCATION = "android.permission.ACCESS_FINE_LOCATION";
const PERM_CAMERA = "android.permission.CAMERA";
const PERM_RECORD_AUDIO = "android.permission.RECORD_AUDIO";

class GeckoViewPermission {
  constructor() {
    this.wrappedJSObject = this;
  }

  _appPermissions = {};

  /* ----------  nsIObserver  ---------- */
  observe(aSubject, aTopic, aData) {
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
  }

  receiveMessage(aMsg) {
    switch (aMsg.name) {
      case "GeckoView:AddCameraPermission": {
        const principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
          aMsg.data.origin
        );

        // Although the lifetime is "session" it will be removed upon
        // use so it's more of a one-shot.
        Services.perms.addFromPrincipal(
          principal,
          "MediaManagerVideo",
          Services.perms.ALLOW_ACTION,
          Services.perms.EXPIRE_SESSION
        );
        break;
      }
    }
  }

  handleMediaAskDevicePermission(aType, aCallback) {
    const perms = [];
    if (aType === "video" || aType === "all") {
      perms.push(PERM_CAMERA);
    }
    if (aType === "audio" || aType === "all") {
      perms.push(PERM_RECORD_AUDIO);
    }

    const [dispatcher] = GeckoViewUtils.getActiveDispatcherAndWindow();
    const callback = _ => {
      Services.obs.notifyObservers(
        aCallback,
        "getUserMedia:got-device-permission"
      );
    };

    if (dispatcher) {
      this.getAppPermissions(dispatcher, perms).then(callback, callback);
    } else {
      // No dispatcher; just bail.
      callback();
    }
  }

  handleMediaRequest(aRequest) {
    const constraints = aRequest.getConstraints();
    const { callID, devices } = aRequest;
    const denyRequest = _ => {
      Services.obs.notifyObservers(null, "getUserMedia:response:deny", callID);
    };

    const win = Services.wm.getOuterWindowWithId(aRequest.windowID);
    // Release the request first.
    aRequest = undefined;
    Promise.resolve()
      .then(() => {
        if (win.closed) {
          return Promise.resolve();
        }

        const sources = devices.map(device => {
          device = device.QueryInterface(Ci.nsIMediaDevice);
          return {
            type: device.type,
            id: device.id,
            rawId: device.rawId,
            name: device.rawName, // unfiltered device name to show to the user
            mediaSource: device.mediaSource,
          };
        });

        if (
          constraints.video &&
          !sources.some(source => source.type === "videoinput")
        ) {
          throw new Error("no video source");
        } else if (
          constraints.audio &&
          !sources.some(source => source.type === "audioinput")
        ) {
          throw new Error("no audio source");
        }

        const dispatcher = GeckoViewUtils.getDispatcherForWindow(win);
        const uri = win.top.document.documentURIObject;
        return dispatcher
          .sendRequestForResult({
            type: "GeckoView:MediaPermission",
            uri: uri.displaySpec,
            video: constraints.video
              ? sources.filter(source => source.type === "videoinput")
              : null,
            audio: constraints.audio
              ? sources.filter(source => source.type === "audioinput")
              : null,
          })
          .then(response => {
            if (!response) {
              // Rejected.
              denyRequest();
              return;
            }
            const allowedDevices = Cc["@mozilla.org/array;1"].createInstance(
              Ci.nsIMutableArray
            );
            if (constraints.video) {
              const video = devices.find(
                device => response.video === device.id
              );
              if (!video) {
                throw new Error("invalid video id");
              }
              Services.cpmm.sendAsyncMessage("GeckoView:AddCameraPermission", {
                origin: win.top.document.nodePrincipal.origin,
              });
              allowedDevices.appendElement(video);
            }
            if (constraints.audio) {
              const audio = devices.find(
                device => response.audio === device.id
              );
              if (!audio) {
                throw new Error("invalid audio id");
              }
              allowedDevices.appendElement(audio);
            }
            Services.obs.notifyObservers(
              allowedDevices,
              "getUserMedia:response:allow",
              callID
            );
          });
      })
      .catch(error => {
        Cu.reportError("Media device error: " + error);
        denyRequest();
      });
  }

  handlePeerConnectionRequest(aRequest) {
    Services.obs.notifyObservers(
      null,
      "PeerConnection:response:allow",
      aRequest.callID
    );
  }

  checkAppPermissions(aPerms) {
    return aPerms.every(perm => this._appPermissions[perm]);
  }

  getAppPermissions(aDispatcher, aPerms) {
    const perms = aPerms.filter(perm => !this._appPermissions[perm]);
    if (!perms.length) {
      return Promise.resolve(/* granted */ true);
    }
    return aDispatcher
      .sendRequestForResult({
        type: "GeckoView:AndroidPermission",
        perms,
      })
      .then(granted => {
        if (granted) {
          for (const perm of perms) {
            this._appPermissions[perm] = true;
          }
        }
        return granted;
      });
  }

  prompt(aRequest) {
    // Only allow exactly one permission request here.
    const types = aRequest.types.QueryInterface(Ci.nsIArray);
    if (types.length !== 1) {
      aRequest.cancel();
      return;
    }

    const perm = types.queryElementAt(0, Ci.nsIContentPermissionType);
    if (
      perm.type === "desktop-notification" &&
      !aRequest.isHandlingUserInput &&
      Services.prefs.getBoolPref(
        "dom.webnotifications.requireuserinteraction",
        true
      )
    ) {
      // We need user interaction and don't have it.
      aRequest.cancel();
      return;
    }

    const dispatcher = GeckoViewUtils.getDispatcherForWindow(
      aRequest.window ? aRequest.window : aRequest.element.ownerGlobal
    );
    dispatcher
      .sendRequestForResult({
        type: "GeckoView:ContentPermission",
        uri: aRequest.principal.URI.displaySpec,
        perm: perm.type,
      })
      .then(granted => {
        if (!granted) {
          return false;
        }
        // Ask for app permission after asking for content permission.
        if (perm.type === "geolocation") {
          return this.getAppPermissions(dispatcher, [
            PERM_ACCESS_FINE_LOCATION,
          ]);
        }
        return true;
      })
      .catch(error => {
        Cu.reportError("Permission error: " + error);
        return /* granted */ false;
      })
      .then(granted => {
        (granted ? aRequest.allow : aRequest.cancel)();
        Services.perms.addFromPrincipal(
          aRequest.principal,
          perm.type,
          granted ? Services.perms.ALLOW_ACTION : Services.perms.DENY_ACTION,
          Services.perms.EXPIRE_SESSION
        );
        // Manually release the target request here to facilitate garbage collection.
        aRequest = undefined;
      });
  }
}

GeckoViewPermission.prototype.classID = Components.ID(
  "{42f3c238-e8e8-4015-9ca2-148723a8afcf}"
);
GeckoViewPermission.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIObserver",
  "nsIContentPermissionPrompt",
]);
