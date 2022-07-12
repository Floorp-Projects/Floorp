/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPermissionChild"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
});

const PERM_ACCESS_FINE_LOCATION = "android.permission.ACCESS_FINE_LOCATION";

class GeckoViewPermissionChild extends GeckoViewActorChild {
  getMediaPermission(aPermission) {
    return this.eventDispatcher.sendRequestForResult({
      type: "GeckoView:MediaPermission",
      ...aPermission,
    });
  }

  addCameraPermission() {
    return this.sendQuery("AddCameraPermission");
  }

  getAppPermissions(aPermissions) {
    return this.sendQuery("GetAppPermissions", aPermissions);
  }

  mediaRecordingStatusChanged(aDevices) {
    return this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaRecordingStatusChanged",
      devices: aDevices,
    });
  }

  async promptPermission(aRequest) {
    // Only allow exactly one permission request here.
    const types = aRequest.types.QueryInterface(Ci.nsIArray);
    if (types.length !== 1) {
      return { allow: false };
    }

    const perm = types.queryElementAt(0, Ci.nsIContentPermissionType);
    if (
      perm.type === "desktop-notification" &&
      !aRequest.hasValidTransientUserGestureActivation &&
      Services.prefs.getBoolPref(
        "dom.webnotifications.requireuserinteraction",
        true
      )
    ) {
      // We need user interaction and don't have it.
      return { allow: false };
    }

    const principal =
      perm.type === "storage-access"
        ? aRequest.principal
        : aRequest.topLevelPrincipal;

    let allowOrDeny;
    try {
      allowOrDeny = await this.eventDispatcher.sendRequestForResult({
        type: "GeckoView:ContentPermission",
        uri: principal.URI.displaySpec,
        thirdPartyOrigin: aRequest.principal.origin,
        principal: lazy.E10SUtils.serializePrincipal(principal),
        perm: perm.type,
        value: perm.capability,
        contextId: principal.originAttributes.geckoViewSessionContextId ?? null,
        privateMode: principal.privateBrowsingId != 0,
      });

      if (allowOrDeny === Services.perms.ALLOW_ACTION) {
        // Ask for app permission after asking for content permission.
        if (perm.type === "geolocation") {
          const granted = await this.getAppPermissions([
            PERM_ACCESS_FINE_LOCATION,
          ]);
          allowOrDeny = granted
            ? Services.perms.ALLOW_ACTION
            : Services.perms.DENY_ACTION;
        }
      }
    } catch (error) {
      Cu.reportError("Permission error: " + error);
      allowOrDeny = Services.perms.DENY_ACTION;
    }

    // Manually release the target request here to facilitate garbage collection.
    aRequest = undefined;

    const allow = allowOrDeny === Services.perms.ALLOW_ACTION;

    // The storage access code adds itself to the perm manager; no need for us to do it.
    if (perm.type === "storage-access") {
      if (allow) {
        return { allow, permission: { "storage-access": "allow" } };
      }
      return { allow };
    }

    Services.perms.addFromPrincipal(
      principal,
      perm.type,
      allowOrDeny,
      Services.perms.EXPIRE_NEVER
    );

    return { allow };
  }
}

const { debug, warn } = GeckoViewPermissionChild.initLogging(
  "GeckoViewPermissionChild"
);
