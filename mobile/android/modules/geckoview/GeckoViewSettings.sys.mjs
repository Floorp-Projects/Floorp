/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewModule } from "resource://gre/modules/GeckoViewModule.sys.mjs";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "MOBILE_USER_AGENT", function () {
  return Cc["@mozilla.org/network/protocol;1?name=http"].getService(
    Ci.nsIHttpProtocolHandler
  ).userAgent;
});

ChromeUtils.defineLazyGetter(lazy, "DESKTOP_USER_AGENT", function () {
  return lazy.MOBILE_USER_AGENT.replace(
    /Android \d.+?; [a-zA-Z]+/,
    "X11; Linux x86_64"
  ).replace(/Gecko\/[0-9\.]+/, "Gecko/20100101");
});

ChromeUtils.defineLazyGetter(lazy, "VR_USER_AGENT", function () {
  return lazy.MOBILE_USER_AGENT.replace(/Mobile/, "Mobile VR");
});

// This needs to match GeckoSessionSettings.java
const USER_AGENT_MODE_MOBILE = 0;
const USER_AGENT_MODE_DESKTOP = 1;
const USER_AGENT_MODE_VR = 2;

// This needs to match GeckoSessionSettings.java
const DISPLAY_MODE_BROWSER = 0;
const DISPLAY_MODE_MINIMAL_UI = 1;
const DISPLAY_MODE_STANDALONE = 2;
const DISPLAY_MODE_FULLSCREEN = 3;

// This needs to match GeckoSessionSettings.java
// eslint-disable-next-line no-unused-vars
const VIEWPORT_MODE_MOBILE = 0;
const VIEWPORT_MODE_DESKTOP = 1;

// Handles GeckoSession settings.
export class GeckoViewSettings extends GeckoViewModule {
  onInit() {
    debug`onInit`;
    this._userAgentMode = USER_AGENT_MODE_MOBILE;
    this._userAgentOverride = null;
    this._sessionContextId = null;

    this.registerListener(["GeckoView:GetUserAgent"]);
  }

  onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:GetUserAgent": {
        aCallback.onSuccess(this.customUserAgent ?? lazy.MOBILE_USER_AGENT);
      }
    }
  }

  onSettingsUpdate() {
    const { settings } = this;
    debug`onSettingsUpdate: ${settings}`;

    this.displayMode = settings.displayMode;
    this.unsafeSessionContextId = settings.unsafeSessionContextId;
    this.userAgentMode = settings.userAgentMode;
    this.userAgentOverride = settings.userAgentOverride;
    this.sessionContextId = settings.sessionContextId;
    this.suspendMediaWhenInactive = settings.suspendMediaWhenInactive;
    this.allowJavascript = settings.allowJavascript;
    this.viewportMode = settings.viewportMode;
    this.useTrackingProtection = !!settings.useTrackingProtection;

    // When the page is loading from the main process (e.g. from an extension
    // page) we won't be able to query the actor here.
    this.getActor("GeckoViewSettings")?.sendAsyncMessage(
      "SettingsUpdate",
      settings
    );
  }

  get allowJavascript() {
    return this.browsingContext.allowJavascript;
  }

  set allowJavascript(aAllowJavascript) {
    this.browsingContext.allowJavascript = aAllowJavascript;
  }

  get customUserAgent() {
    if (this.userAgentOverride !== null) {
      return this.userAgentOverride;
    }
    if (this.userAgentMode === USER_AGENT_MODE_DESKTOP) {
      return lazy.DESKTOP_USER_AGENT;
    }
    if (this.userAgentMode === USER_AGENT_MODE_VR) {
      return lazy.VR_USER_AGENT;
    }
    return null;
  }

  set useTrackingProtection(aUse) {
    this.browsingContext.useTrackingProtection = aUse;
  }

  set viewportMode(aViewportMode) {
    this.browsingContext.forceDesktopViewport =
      aViewportMode == VIEWPORT_MODE_DESKTOP;
  }

  get userAgentMode() {
    return this._userAgentMode;
  }

  set userAgentMode(aMode) {
    if (this.userAgentMode === aMode) {
      return;
    }
    this._userAgentMode = aMode;
    this.browsingContext.customUserAgent = this.customUserAgent;
  }

  get browsingContext() {
    return this.browser.browsingContext.top;
  }

  get userAgentOverride() {
    return this._userAgentOverride;
  }

  set userAgentOverride(aUserAgent) {
    if (aUserAgent === this.userAgentOverride) {
      return;
    }
    this._userAgentOverride = aUserAgent;
    this.browsingContext.customUserAgent = this.customUserAgent;
  }

  get suspendMediaWhenInactive() {
    return this.browser.suspendMediaWhenInactive;
  }

  set suspendMediaWhenInactive(aSuspendMediaWhenInactive) {
    if (aSuspendMediaWhenInactive != this.browser.suspendMediaWhenInactive) {
      this.browser.suspendMediaWhenInactive = aSuspendMediaWhenInactive;
    }
  }

  displayModeSettingToValue(aSetting) {
    switch (aSetting) {
      case DISPLAY_MODE_BROWSER:
        return "browser";
      case DISPLAY_MODE_MINIMAL_UI:
        return "minimal-ui";
      case DISPLAY_MODE_STANDALONE:
        return "standalone";
      case DISPLAY_MODE_FULLSCREEN:
        return "fullscreen";
      default:
        warn`Invalid displayMode value ${aSetting}.`;
        return "browser";
    }
  }

  set displayMode(aMode) {
    this.browsingContext.displayMode = this.displayModeSettingToValue(aMode);
  }

  set sessionContextId(aAttribute) {
    this._sessionContextId = aAttribute;
  }

  get sessionContextId() {
    return this._sessionContextId;
  }
}

const { debug, warn } = GeckoViewSettings.initLogging("GeckoViewSettings");
