/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const EXPORTED_SYMBOLS = ["ProgressDelegateChild"];

class ProgressDelegateChild extends GeckoViewActorChild {
  // eslint-disable-next-line complexity
  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;
    switch (aEvent.type) {
      case "DOMContentLoaded": // fall-through
      case "MozAfterPaint": // fall-through
      case "pageshow": {
        // Forward to main process
        const target = aEvent.originalTarget;
        const uri = target?.location.href;
        this.sendAsyncMessage(aEvent.type, {
          uri,
        });
      }
    }
  }
}

const { debug, warn } = ProgressDelegateChild.initLogging("ProgressDelegate");
