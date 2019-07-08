/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewChildModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewChildModule.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "MOBILE_USER_AGENT", function() {
  return Cc["@mozilla.org/network/protocol;1?name=http"].getService(
    Ci.nsIHttpProtocolHandler
  ).userAgent;
});

XPCOMUtils.defineLazyGetter(this, "DESKTOP_USER_AGENT", function() {
  return MOBILE_USER_AGENT.replace(
    /Android \d.+?; [a-zA-Z]+/,
    "X11; Linux x86_64"
  ).replace(/Gecko\/[0-9\.]+/, "Gecko/20100101");
});

XPCOMUtils.defineLazyGetter(this, "VR_USER_AGENT", function() {
  return MOBILE_USER_AGENT.replace(/Mobile/, "Mobile VR");
});

// This needs to match GeckoSessionSettings.java
const USER_AGENT_MODE_MOBILE = 0;
const USER_AGENT_MODE_DESKTOP = 1;
const USER_AGENT_MODE_VR = 2;

// This needs to match GeckoSessionSettings.java
const VIEWPORT_MODE_MOBILE = 0;
const VIEWPORT_MODE_DESKTOP = 1;

// Handles GeckoView content settings including:
// * tracking protection
// * user agent mode
class GeckoViewSettingsChild extends GeckoViewChildModule {
  onInit() {
    debug`onInit`;
    this._userAgentMode = USER_AGENT_MODE_MOBILE;
    this._userAgentOverride = null;
    this._viewportMode = VIEWPORT_MODE_MOBILE;
  }

  onSettingsUpdate() {
    debug`onSettingsUpdate ${this.settings}`;

    this.displayMode = this.settings.displayMode;
    this.useTrackingProtection = !!this.settings.useTrackingProtection;
    this.userAgentMode = this.settings.userAgentMode;
    this.userAgentOverride = this.settings.userAgentOverride;
    this.viewportMode = this.settings.viewportMode;
    this.allowJavascript = this.settings.allowJavascript;
  }

  get useTrackingProtection() {
    return docShell.useTrackingProtection;
  }

  set useTrackingProtection(aUse) {
    docShell.useTrackingProtection = aUse;
  }

  get userAgent() {
    if (this.userAgentOverride !== null) {
      return this.userAgentOverride;
    }
    if (this.userAgentMode === USER_AGENT_MODE_DESKTOP) {
      return DESKTOP_USER_AGENT;
    }
    if (this.userAgentMode === USER_AGENT_MODE_VR) {
      return VR_USER_AGENT;
    }
    return null;
  }

  get userAgentMode() {
    return this._userAgentMode;
  }

  set userAgentMode(aMode) {
    if (this.userAgentMode === aMode) {
      return;
    }
    this._userAgentMode = aMode;
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    if (docShell) {
      docShell.customUserAgent = this.userAgent;
    } else {
      warn`Failed to set custom user agent. Doc shell not found`;
    }
  }

  get userAgentOverride() {
    return this._userAgentOverride;
  }

  set userAgentOverride(aUserAgent) {
    if (aUserAgent === this._userAgentOverride) {
      return;
    }
    this._userAgentOverride = aUserAgent;
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    if (docShell) {
      docShell.customUserAgent = this.userAgent;
    } else {
      warn`Failed to set custom user agent. Doc shell not found`;
    }
  }

  get displayMode() {
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    return docShell
      ? docShell.displayMode
      : Ci.nsIDocShell.DISPLAY_MODE_BROWSER;
  }

  set displayMode(aMode) {
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    if (docShell) {
      docShell.displayMode = aMode;
    }
  }

  get viewportMode() {
    return this._viewportMode;
  }

  set viewportMode(aMode) {
    if (aMode === this._viewportMode) {
      return;
    }
    this._viewportMode = aMode;
    const utils = content.windowUtils;
    utils.setDesktopModeViewport(aMode === VIEWPORT_MODE_DESKTOP);
  }

  get allowJavascript() {
    return docShell.allowJavascript;
  }

  set allowJavascript(aAllowJavascript) {
    docShell.allowJavascript = aAllowJavascript;
  }
}

const { debug, warn } = GeckoViewSettingsChild.initLogging("GeckoViewSettings"); // eslint-disable-line no-unused-vars
const module = GeckoViewSettingsChild.create(this);
