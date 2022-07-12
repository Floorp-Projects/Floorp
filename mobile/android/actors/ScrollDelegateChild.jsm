/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const EXPORTED_SYMBOLS = ["ScrollDelegateChild"];

class ScrollDelegateChild extends GeckoViewActorChild {
  // eslint-disable-next-line complexity
  handleEvent(aEvent) {
    if (aEvent.originalTarget.ownerGlobal != this.contentWindow) {
      return;
    }

    debug`handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "mozvisualscroll":
        const x = {};
        const y = {};
        this.contentWindow.windowUtils.getVisualViewportOffset(x, y);
        this.eventDispatcher.sendRequest({
          type: "GeckoView:ScrollChanged",
          scrollX: x.value,
          scrollY: y.value,
        });
        break;
    }
  }
}

const { debug, warn } = ScrollDelegateChild.initLogging("ScrollDelegate");
