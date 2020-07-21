/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["MarionetteFrameChild"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Log } = ChromeUtils.import("chrome://marionette/content/log.js");

XPCOMUtils.defineLazyGetter(this, "logger", Log.get);

class MarionetteFrameChild extends JSWindowActorChild {
  get id() {
    return this.browsingContext.id;
  }

  get innerWindowId() {
    return this.manager.innerWindowId;
  }

  actorCreated() {
    logger.trace(
      `[${this.id}] Child actor created for window id ${this.innerWindowId}`
    );
  }

  handleEvent(event) {
    switch (event.type) {
      case "beforeunload":
      case "pagehide":
      case "DOMContentLoaded":
      case "pageshow":
        this.sendAsyncMessage("MarionetteFrameChild:PageLoadEvent", {
          browsingContext: this.browsingContext,
          innerWindowId: this.innerWindowId,
          type: event.type,
        });
        break;
    }
  }
}
