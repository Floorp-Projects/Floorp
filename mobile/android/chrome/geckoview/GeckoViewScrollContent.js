/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "ViewScrollContent");

function debug(aMsg) {
  // dump(aMsg);
}

var ScrollListener = {
  init: function() {
    addEventListener("scroll", this, false);
  },

  handleEvent: function(aEvent) {
    if (aEvent.originalTarget.defaultView != content) {
      return;
    }

    debug("handleEvent " + aEvent.type);

    switch (aEvent.type) {
      case "scroll":
        sendAsyncMessage("GeckoView:ScrollChanged",
                         { scrollX: Math.round(content.scrollX),
                           scrollY: Math.round(content.scrollY) });
        break;
    }
  },
};

ScrollListener.init();
