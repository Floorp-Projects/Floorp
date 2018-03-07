/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewSettings"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  SafeBrowsing: "resource://gre/modules/SafeBrowsing.jsm",
  UserAgentOverrides: "resource://gre/modules/UserAgentOverrides.jsm",
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
  init() {
    this._isSafeBrowsingInit = false;

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
