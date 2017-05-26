/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.d.bind(null, "ViewContent");

function debug(aMsg) {
  // dump(aMsg);
}

// This is copied from desktop's tab-content.js. See bug 1153485 about sharing this code somehow.
var DOMTitleChangedListener = {
  init: function() {
    addEventListener("DOMTitleChanged", this, false);
    addEventListener("MozDOMFullscreen:Entered", this, false);
    addEventListener("MozDOMFullscreen:Exit", this, false);
    addEventListener("MozDOMFullscreen:Exited", this, false);
    addEventListener("MozDOMFullscreen:Request", this, false);

    addMessageListener("GeckoView:DOMFullscreenEntered", this);
    addMessageListener("GeckoView:DOMFullscreenExited", this);
  },

  receiveMessage: function(aMsg) {
    debug("receiveMessage " + aMsg.name);

    switch (aMsg.name) {
      case "GeckoView:DOMFullscreenEntered":
        if (content) {
          content.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindowUtils)
                 .handleFullscreenRequests();
        }
        break;
      case "GeckoView:DOMFullscreenExited":
        if (content) {
          content.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindowUtils)
                 .exitFullscreen();
        }
        break;
    }
  },

  handleEvent: function(aEvent) {
    if (aEvent.originalTarget.defaultView != content) {
      return;
    }

    debug("handleEvent " + aEvent.type);

    switch (aEvent.type) {
      case "MozDOMFullscreen:Request":
        sendAsyncMessage("GeckoView:DOMFullscreenRequest");
        break;
      case "MozDOMFullscreen:Entered":
      case "MozDOMFullscreen:Exited":
        // Content may change fullscreen state by itself, and we should ensure
        // that the parent always exits fullscreen when content has left
        // full screen mode.
        if (content && content.document.fullscreenElement) {
          break;
        }
        // fall-through
      case "MozDOMFullscreen:Exit":
        sendAsyncMessage("GeckoView:DOMFullscreenExit");
        break;
      case "DOMTitleChanged":
        sendAsyncMessage("GeckoView:DOMTitleChanged",
                         { title: content.document.title });
        break;
    }
  },
};

DOMTitleChangedListener.init();
