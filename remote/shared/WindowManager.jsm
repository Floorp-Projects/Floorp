/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["WindowManager"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",

  EventPromise: "chrome://remote/content/shared/Sync.jsm",
});

var WindowManager = {
  async focus(window) {
    if (window != Services.focus.activeWindow) {
      const promises = [
        EventPromise(window, "activate"),
        EventPromise(window, "focus", { capture: true }),
      ];

      window.focus();

      await Promise.all(promises);
    }
  },
};
