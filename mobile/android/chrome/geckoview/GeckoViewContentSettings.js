/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/GeckoViewContentModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "dump", () =>
    ChromeUtils.import("resource://gre/modules/AndroidLog.jsm",
                       {}).AndroidLog.d.bind(null, "ViewSettings[C]"));

function debug(aMsg) {
  // dump(aMsg);
}

// Handles GeckoView content settings including:
// * tracking protection
// * desktop mode
class GeckoViewContentSettings extends GeckoViewContentModule {
  init() {
    debug("init");
    this._useTrackingProtection = false;
    this._useDesktopMode = false;
  }

  onSettingsUpdate() {
    debug("onSettingsUpdate");

    this.useTrackingProtection = !!this.settings.useTrackingProtection;
    this.useDesktopMode = !!this.settings.useDesktopMode;
  }

  get useTrackingProtection() {
    return this._useTrackingProtection;
  }

  set useTrackingProtection(aUse) {
    if (aUse != this._useTrackingProtection) {
      docShell.useTrackingProtection = aUse;
      this._useTrackingProtection = aUse;
    }
  }

  get useDesktopMode() {
    return this._useDesktopMode;
  }

  set useDesktopMode(aUse) {
    if (this.useDesktopMode === aUse) {
      return;
    }
    let utils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIDOMWindowUtils);
    utils.setDesktopModeViewport(aUse);
    this._useDesktopMode = aUse;
  }
}

var settings = new GeckoViewContentSettings("GeckoViewSettings", this);
