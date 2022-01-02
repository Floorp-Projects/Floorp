/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const EXPORTED_SYMBOLS = ["GeckoViewSettingsChild"];

// This needs to match GeckoSessionSettings.java
const VIEWPORT_MODE_MOBILE = 0;
const VIEWPORT_MODE_DESKTOP = 1;

// Handles GeckoView content settings
class GeckoViewSettingsChild extends GeckoViewActorChild {
  receiveMessage(message) {
    const { name } = message;
    debug`receiveMessage: ${name}`;

    switch (name) {
      case "SettingsUpdate": {
        const settings = message.data;

        this.viewportMode = settings.viewportMode;
        if (settings.isPopup) {
          // Allow web extensions to close their own action popups (bz1612363)
          this.contentWindow.windowUtils.allowScriptsToClose();
        }
      }
    }
  }

  set viewportMode(aMode) {
    const { windowUtils } = this.contentWindow;
    if (aMode === windowUtils.desktopModeViewport) {
      return;
    }
    windowUtils.desktopModeViewport = aMode === VIEWPORT_MODE_DESKTOP;
  }
}

const { debug, warn } = GeckoViewSettingsChild.initLogging("GeckoViewSettings");
