/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewChildModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewChildModule.jsm"
);

class GeckoViewScrollChild extends GeckoViewChildModule {
  onEnable() {
    debug`onEnable`;
    addEventListener("mozvisualscroll", this, { mozSystemGroup: true });
  }

  onDisable() {
    debug`onDisable`;
    removeEventListener("mozvisualscroll", { mozSystemGroup: true });
  }

  handleEvent(aEvent) {
    if (aEvent.originalTarget.ownerGlobal != content) {
      return;
    }

    debug`handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "mozvisualscroll":
        const x = {},
          y = {};
        content.windowUtils.getVisualViewportOffset(x, y);
        this.eventDispatcher.sendRequest({
          type: "GeckoView:ScrollChanged",
          scrollX: x.value,
          scrollY: y.value,
        });
        break;
    }
  }
}

const { debug, warn } = GeckoViewScrollChild.initLogging("GeckoViewScroll"); // eslint-disable-line no-unused-vars
const module = GeckoViewScrollChild.create(this);
