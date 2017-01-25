/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.d.bind(null, "ViewContent");

var DEBUG = false;

// This is copied from desktop's tab-content.js. See bug 1153485 about sharing this code somehow.
var DOMTitleChangedListener = {
  init: function() {
    addEventListener("DOMTitleChanged", this, false);
  },

  receiveMessage: function(message) {
    if (DEBUG) {
      dump("receiveMessage " + message.name);
    }
  },

  handleEvent: function(aEvent) {
    if (aEvent.originalTarget.defaultView != content) {
      return;
    }

    if (DEBUG) {
      dump("handleEvent " + aEvent.type);
    }

    switch (aEvent.type) {
      case "DOMTitleChanged":
        sendAsyncMessage("GeckoView:DOMTitleChanged", { title: content.document.title });
        break;
    }
  },
};

DOMTitleChangedListener.init();
