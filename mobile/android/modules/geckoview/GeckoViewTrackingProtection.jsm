/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewTrackingProtection"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

class GeckoViewTrackingProtection extends GeckoViewModule {
  onEnable() {
    debug `onEnable`;

    const flags = Ci.nsIWebProgress.NOTIFY_SECURITY;
    this.progressFilter =
      Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
      .createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    this.browser.addProgressListener(this.progressFilter, flags);
  }

  onSecurityChange(aWebProgress, aRequest, aState) {
    debug `onSecurityChange`;

    if (!(aState & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT) ||
        !aRequest || !(aRequest instanceof Ci.nsIClassifiedChannel)) {
      return;
    }

    let channel = aRequest.QueryInterface(Ci.nsIChannel);
    let uri = channel.URI && channel.URI.spec;
    let classChannel = aRequest.QueryInterface(Ci.nsIClassifiedChannel);

    if (!uri || !classChannel.matchedList) {
      return;
    }

    let message = {
      type: "GeckoView:TrackingProtectionBlocked",
      src: uri,
      matchedList: classChannel.matchedList
    };

    this.eventDispatcher.sendRequest(message);
  }
}
