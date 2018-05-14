/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewSettings"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

/* global SafeBrowsing:false */
GeckoViewUtils.addLazyGetter(this, "SafeBrowsing", {
  module: "resource://gre/modules/SafeBrowsing.jsm",
  init: sb => sb.init(),
});

XPCOMUtils.defineLazyGetter(
  this, "DESKTOP_USER_AGENT",
  function() {
    return Cc["@mozilla.org/network/protocol;1?name=http"]
           .getService(Ci.nsIHttpProtocolHandler).userAgent
           .replace(/Android \d.+?; [a-zA-Z]+/, "X11; Linux x86_64")
           .replace(/Gecko\/[0-9\.]+/, "Gecko/20100101");
  });

// Handles GeckoView settings including:
// * multiprocess
// * user agent override
class GeckoViewSettings extends GeckoViewModule {
  onInitBrowser() {
    if (this.settings.useMultiprocess) {
      this.browser.setAttribute("remote", "true");
    }
  }

  onInit() {
    this._useTrackingProtection = false;
    this._useDesktopMode = false;
  }

  onSettingsUpdate() {
    const settings = this.settings;
    debug `onSettingsUpdate: ${settings}`;

    this.displayMode = settings.displayMode;
    this.useTrackingProtection = !!settings.useTrackingProtection;
    this.useDesktopMode = !!settings.useDesktopMode;
  }

  get useMultiprocess() {
    return this.browser.isRemoteBrowser;
  }

  get useTrackingProtection() {
    return this._useTrackingProtection;
  }

  set useTrackingProtection(aUse) {
    aUse && SafeBrowsing;
    this._useTrackingProtection = aUse;
  }

  onUserAgentRequest(aSubject, aTopic, aData) {
    debug `onUserAgentRequest`;

    let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);

    if (this.browser.outerWindowID !== channel.topLevelOuterContentWindowId) {
      return;
    }

    if (this.useDesktopMode) {
      channel.setRequestHeader("User-Agent", DESKTOP_USER_AGENT, false);
    }
  }

  get useDesktopMode() {
    return this._useDesktopMode;
  }

  set useDesktopMode(aUse) {
    if (this.useDesktopMode === aUse) {
      return;
    }
    if (aUse) {
      Services.obs.addObserver(this.onUserAgentRequest.bind(this),
                               "http-on-useragent-request");
    } else {
      Services.obs.removeObserver(this.onUserAgentRequest.bind(this),
                                  "http-on-useragent-request");
    }
    this._useDesktopMode = aUse;
  }

  get displayMode() {
    return this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDocShell)
                      .displayMode;
  }

  set displayMode(aMode) {
    this.window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDocShell)
               .displayMode = aMode;
  }
}
