/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewAccessibility"];

const { GeckoViewModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewModule.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  AccessFu: "resource://gre/modules/accessibility/AccessFu.jsm",
});

class GeckoViewAccessibility extends GeckoViewModule {
  onInit() {
    EventDispatcher.instance.registerListener((aEvent, aData, aCallback) => {
      if (aData.touchEnabled) {
        AccessFu.enable();
      } else {
        AccessFu.disable();
      }
    }, "GeckoView:AccessibilitySettings");
  }
}

// eslint-disable-next-line no-unused-vars
const { debug, warn } = GeckoViewAccessibility.initLogging(
  "GeckoViewAccessibility"
);
