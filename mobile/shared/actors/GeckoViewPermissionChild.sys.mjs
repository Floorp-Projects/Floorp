/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewActorChild } from "resource://gre/modules/GeckoViewActorChild.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  E10SUtils: "resource://gre/modules/E10SUtils.sys.mjs",
});

const PERM_ACCESS_FINE_LOCATION = "android.permission.ACCESS_FINE_LOCATION";

const MAPPED_TO_EXTENSION_PERMISSIONS = [
  "persistent-storage",
  // TODO(Bug 1336194): support geolocation manifest permission
  // (see https://bugzilla.mozilla.org/show_bug.cgi?id=1336194#c17)l
];

export class GeckoViewPermissionChild extends GeckoViewActorChild {
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

  // Some WebAPI permissions can be requested and granted to extensions through the
  // the extension manifest.json, which the user have been already prompted for
  // (e.g. at install time for the one listed in the manifest.json permissions property,
  // or at runtime through the optional_permissions property and the permissions.request
  // WebExtensions API method).
  //
  // WebAPI permission that are expected to be mapped to extensions permissions are listed
  // in the MAPPED_TO_EXTENSION_PERMISSIONS array.
  //
  // @param {nsIContentPermissionType} perm
  //        The WebAPI permission being requested
  // @param {nsIContentPermissionRequest} aRequest
  //        The nsIContentPermissionRequest as received by the promptPermission method.
  //
  // @returns {null | { allow: boolean, permission: Object }
  //          Returns null if the request was not handled and should continue with the
  //          regular permission prompting flow, otherwise it returns an object to
  //          allow or disallow the permission request right away.
  checkIfGrantedByExtensionPermissions(perm, aRequest) {
    if (!aRequest.principal.addonPolicy) {
      // Not an extension, continue with the regular permission prompting flow.
      return null;
    }

    // Return earlier and continue with the regular permission prompting flow if the
    // the permission is no one that can be requested from the extension manifest file.
    if (!MAPPED_TO_EXTENSION_PERMISSIONS.includes(perm.type)) {
      return null;
    }

    // Disallow if the extension is not active anymore.
    if (!aRequest.principal.addonPolicy.active) {
      return { allow: false };
    }

    // Check if the permission have been already granted to the extension, if it is allow it right away.
    const isGranted =
      Services.perms.testPermissionFromPrincipal(
        aRequest.principal,
        perm.type
      ) === Services.perms.ALLOW_ACTION;
    if (isGranted) {
      return {
        allow: true,
        permission: { [perm.type]: Services.perms.ALLOW_ACTION },
      };
    }

    // continue with the regular permission prompting flow otherwise.
    return null;
  }

  async promptPermission(aRequest) {
    // Only allow exactly one permission request here.
    const types = aRequest.types.QueryInterface(Ci.nsIArray);
    if (types.length !== 1) {
      return { allow: false };
    }

    const perm = types.queryElementAt(0, Ci.nsIContentPermissionType);

    // Check if the request principal is an extension principal and if the permission requested
    // should be already granted based on the extension permissions (or disallowed right away
    // because the extension is not enabled anymore.
    const extensionResult = this.checkIfGrantedByExtensionPermissions(
      perm,
      aRequest
    );
    if (extensionResult) {
      return extensionResult;
    }

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
      console.error("Permission error:", error);
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
