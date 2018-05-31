/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewAccessibility"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  AccessFu: "resource://gre/modules/accessibility/AccessFu.jsm"
});

class GeckoViewAccessibility extends GeckoViewModule {
  onInit() {
    EventDispatcher.instance.dispatch("GeckoView:AccessibilityReady");
    EventDispatcher.instance.registerListener((aEvent, aData, aCallback) => {
      if (aData.enabled) {
        AccessFu.enable();
      } else {
        AccessFu.disable();
      }
    }, "GeckoView:AccessibilitySettings");
  }
}
