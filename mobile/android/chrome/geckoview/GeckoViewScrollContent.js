/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/GeckoViewContentModule.jsm");

class GeckoViewScrollContent extends GeckoViewContentModule {
  onEnable() {
    debug `onEnable`;
    addEventListener("scroll", this, false);
  }

  onDisable() {
    debug `onDisable`;
    removeEventListener("scroll", this);
  }

  handleEvent(aEvent) {
    if (aEvent.originalTarget.defaultView != content) {
      return;
    }

    debug `handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "scroll":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:ScrollChanged",
          scrollX: Math.round(content.scrollX),
          scrollY: Math.round(content.scrollY)
        });
        break;
    }
  }
}

let {debug, warn} = GeckoViewScrollContent.initLogging("GeckoViewScroll");
let module = GeckoViewScrollContent.create(this);
