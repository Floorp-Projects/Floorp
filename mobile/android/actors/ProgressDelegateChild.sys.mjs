/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewActorChild } from "resource://gre/modules/GeckoViewActorChild.sys.mjs";

export class ProgressDelegateChild extends GeckoViewActorChild {
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
