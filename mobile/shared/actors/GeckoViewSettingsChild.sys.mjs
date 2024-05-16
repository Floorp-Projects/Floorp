/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewActorChild } from "resource://gre/modules/GeckoViewActorChild.sys.mjs";

// Handles GeckoView content settings
export class GeckoViewSettingsChild extends GeckoViewActorChild {
  receiveMessage(message) {
    const { name } = message;
    debug`receiveMessage: ${name}`;

    switch (name) {
      case "SettingsUpdate": {
        const settings = message.data;

        if (settings.isExtensionPopup) {
          // Allow web extensions to close their own action popups (bz1612363)
          this.contentWindow.windowUtils.allowScriptsToClose();
        }
      }
    }
  }
}

const { debug, warn } = GeckoViewSettingsChild.initLogging("GeckoViewSettings");
