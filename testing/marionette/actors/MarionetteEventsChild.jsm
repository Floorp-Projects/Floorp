/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-restricted-globals */

"use strict";

const EXPORTED_SYMBOLS = ["MarionetteEventsChild"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  event: "chrome://marionette/content/event.js",
  Log: "chrome://marionette/content/log.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

class MarionetteEventsChild extends JSWindowActorChild {
  get innerWindowId() {
    return this.manager.innerWindowId;
  }

  actorCreated() {
    logger.trace(
      `[${this.browsingContext.id}] MarionetteEvents actor created ` +
        `for window id ${this.innerWindowId}`
    );
  }

  handleEvent({ target, type }) {
    // Ignore invalid combinations of load events and document's readyState.
    if (
      (type === "DOMContentLoaded" && target.readyState != "interactive") ||
      (type === "pageshow" && target.readyState != "complete")
    ) {
      logger.warn(
        `Ignoring event '${type}' because document has an invalid ` +
          `readyState of '${target.readyState}'.`
      );
      return;
    }

    switch (type) {
      case "beforeunload":
      case "DOMContentLoaded":
      case "hashchange":
      case "pagehide":
      case "pageshow":
      case "popstate":
        this.sendAsyncMessage("MarionetteEventsChild:PageLoadEvent", {
          browsingContext: this.browsingContext,
          documentURI: target.documentURI,
          readyState: target.readyState,
          type,
          windowId: this.innerWindowId,
        });
        break;

      // Listen for click event to indicate one click has happened, so actions
      // code can send dblclick event
      case "click":
        event.DoubleClickTracker.setClick();
        break;
      case "dblclick":
      case "unload":
        event.DoubleClickTracker.resetClick();
        break;
    }
  }
}
