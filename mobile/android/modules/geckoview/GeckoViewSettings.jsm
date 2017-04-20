/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GeckoViewSettings"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/GeckoViewModule.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SafeBrowsing",
  "resource://gre/modules/SafeBrowsing.jsm");

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "ViewSettings");

function debug(aMsg) {
  // dump(aMsg);
}

// Handles GeckoView settings including:
// * tracking protection
// * multiprocess
class GeckoViewSettings extends GeckoViewModule {
  init() {
    this._isSafeBrowsingInit = false;
    this._useTrackingProtection = false;

    // We only allow to set this setting during initialization, further updates
    // will be ignored.
    this.useMultiprocess = !!this.settings.useMultiprocess;
  }

  onSettingsUpdate() {
    debug("onSettingsUpdate: " + JSON.stringify(this.settings));

    this.useTrackingProtection = !!this.settings.useTrackingProtection;
  }

  get useTrackingProtection() {
    return this._useTrackingProtection;
  }

  set useTrackingProtection(aUse) {
    if (aUse && !this._isSafeBrowsingInit) {
      SafeBrowsing.init();
      this._isSafeBrowsingInit = true;
    }
    if (aUse != this._useTrackingProtection) {
      this.messageManager.loadFrameScript('data:,' +
        'docShell.useTrackingProtection = ' + aUse,
        true
      );
      this._useTrackingProtection = aUse;
    }
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
}
