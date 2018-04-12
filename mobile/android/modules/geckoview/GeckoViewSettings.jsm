/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewSettings"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  SafeBrowsing: "resource://gre/modules/SafeBrowsing.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyGetter(
  this, "DESKTOP_USER_AGENT",
  function() {
    return Cc["@mozilla.org/network/protocol;1?name=http"]
           .getService(Ci.nsIHttpProtocolHandler).userAgent
           .replace(/Android \d.+?; [a-zA-Z]+/, "X11; Linux x86_64")
           .replace(/Gecko\/[0-9\.]+/, "Gecko/20100101");
  });

XPCOMUtils.defineLazyGetter(this, "dump", () =>
    ChromeUtils.import("resource://gre/modules/AndroidLog.jsm",
                       {}).AndroidLog.d.bind(null, "ViewSettings"));

function debug(aMsg) {
  // dump(aMsg);
}

// Handles GeckoView settings including:
// * multiprocess
// * user agent override
class GeckoViewSettings extends GeckoViewModule {
  onInit() {
    this._isSafeBrowsingInit = false;
    this._useDesktopMode = false;

    // We only allow to set this setting during initialization, further updates
    // will be ignored.
    this.useMultiprocess = !!this.settings.useMultiprocess;
    this._displayMode = Ci.nsIDocShell.DISPLAY_MODE_BROWSER;

    this.messageManager.loadFrameScript(
      "chrome://geckoview/content/GeckoViewContentSettings.js", true);
  }

  onSettingsUpdate() {
    debug("onSettingsUpdate: " + JSON.stringify(this.settings));

    this.displayMode = this.settings.displayMode;
    this.useTrackingProtection = !!this.settings.useTrackingProtection;
    this.useDesktopMode = !!this.settings.useDesktopMode;
  }

  get useMultiprocess() {
    return this.browser.getAttribute("remote") == "true";
  }

  set useMultiprocess(aUse) {
    if (aUse == this.useMultiprocess) {
      return;
    }
    let parentNode = this.browser.parentNode;
    parentNode.removeChild(this.browser);

    if (aUse) {
      this.browser.setAttribute("remote", "true");
    } else {
      this.browser.removeAttribute("remote");
    }
    parentNode.appendChild(this.browser);
  }

  set useTrackingProtection(aUse) {
    if (aUse && !this._isSafeBrowsingInit) {
      SafeBrowsing.init();
      this._isSafeBrowsingInit = true;
    }
  }

  onUserAgentRequest(aSubject, aTopic, aData) {
    debug("onUserAgentRequest");

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
    return this._displayMode;
  }

  set displayMode(aMode) {
    if (!this.useMultiprocess) {
      this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDocShell)
                   .displayMode = aMode;
    } else {
      this.messageManager.loadFrameScript("data:," +
        `docShell.displayMode = ${aMode}`,
        true
      );
    }
    this._displayMode = aMode;
  }
}
