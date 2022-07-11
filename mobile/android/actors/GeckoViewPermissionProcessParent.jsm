/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPermissionProcessParent"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

// See: http://developer.android.com/reference/android/Manifest.permission.html
const PERM_CAMERA = "android.permission.CAMERA";
const PERM_RECORD_AUDIO = "android.permission.RECORD_AUDIO";

class GeckoViewPermissionProcessParent extends JSProcessActorParent {
  async askDevicePermission(aType) {
    const perms = [];
    if (aType === "video" || aType === "all") {
      perms.push(PERM_CAMERA);
    }
    if (aType === "audio" || aType === "all") {
      perms.push(PERM_RECORD_AUDIO);
    }

    try {
      // This looks sketchy but it's fine: Android needs the audio/video
      // permission to enumerate devices, which Gecko wants to do even before
      // we expose the list to web pages.
      // So really it doesn't matter what the request source is, because we
      // will, separately, issue a windowId-specific request to let the webpage
      // actually have access to the list of devices. So even if the source of
      // *this* request is incorrect, no actual harm will be done, as the user
      // will always receive the request with the right origin after this.
      const window = Services.wm.getMostRecentWindow("navigator:geckoview");
      const windowActor = window.browsingContext.currentWindowGlobal.getActor(
        "GeckoViewPermission"
      );
      await windowActor.getAppPermissions(perms);
    } catch (error) {
      // We can't really do anything here so we pretend we got the permission.
      warn`Error getting app permission: ${error}`;
    }
  }

  receiveMessage(aMessage) {
    debug`receiveMessage ${aMessage.name}`;

    switch (aMessage.name) {
      case "AskDevicePermission": {
        return this.askDevicePermission(aMessage.data);
      }
    }

    return super.receiveMessage(aMessage);
  }
}

const { debug, warn } = GeckoViewUtils.initLogging(
  "GeckoViewPermissionProcess"
);
