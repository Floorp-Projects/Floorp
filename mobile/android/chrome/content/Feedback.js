/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Feedback = {
  observe: function(aMessage, aTopic, aData) {
    if (aTopic !== "Feedback:Show")
      return;

    // Only prompt for feedback if this isn't a distribution build.
    try {
      Services.prefs.getCharPref("distribution.id");
    } catch (e) {
      BrowserApp.addTab("about:feedback?source=feedback-prompt", { selected: true, parentId: BrowserApp.selectedTab.id });
    }
  }
};
