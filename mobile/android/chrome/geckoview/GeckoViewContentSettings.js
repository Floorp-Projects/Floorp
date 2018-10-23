/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/GeckoViewContentModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
});

// This needs to match GeckoSessionSettings.java
const USER_AGENT_MODE_MOBILE = 0;

// Handles GeckoView content settings including:
// * tracking protection
// * user agent mode
class GeckoViewContentSettings extends GeckoViewContentModule {
  onInit() {
    debug `onInit`;
    this._userAgentMode = USER_AGENT_MODE_MOBILE;
  }

  onSettingsUpdate() {
    debug `onSettingsUpdate ${this.settings}`;

    this.displayMode = this.settings.displayMode;
    this.useTrackingProtection = !!this.settings.useTrackingProtection;
    this.userAgentMode = this.settings.userAgentMode;
    this.allowJavascript = this.settings.allowJavascript;
  }

  get useTrackingProtection() {
    return docShell.useTrackingProtection;
  }

  set useTrackingProtection(aUse) {
    docShell.useTrackingProtection = aUse;
  }

  get userAgentMode() {
    return this._userAgentMode;
  }

  set userAgentMode(aMode) {
    if (this.userAgentMode === aMode) {
      return;
    }
    let utils = content.windowUtils;
    utils.setDesktopModeViewport(aMode != USER_AGENT_MODE_MOBILE);
    this._userAgentMode = aMode;
  }

  get displayMode() {
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    return docShell ? docShell.displayMode
                    : Ci.nsIDocShell.DISPLAY_MODE_BROWSER;
  }

  set displayMode(aMode) {
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    if (docShell) {
      docShell.displayMode = aMode;
    }
  }

  get allowJavascript() {
    return docShell.allowJavascript;
  }

  set allowJavascript(aAllowJavascript) {
    docShell.allowJavascript = aAllowJavascript;
  }
}

let {debug, warn} = GeckoViewContentSettings.initLogging("GeckoViewSettings");
let module = GeckoViewContentSettings.create(this);
