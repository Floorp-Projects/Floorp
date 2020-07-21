/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["MarionetteFrameParent"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

const { Log } = ChromeUtils.import("chrome://marionette/content/log.js");

XPCOMUtils.defineLazyGetter(this, "logger", Log.get);

class MarionetteFrameParent extends JSWindowActorParent {
  constructor() {
    super();

    EventEmitter.decorate(this);
  }

  actorCreated() {
    logger.trace(`[${this.browsingContext.id}] Parent actor created`);
  }

  receiveMessage({ name, data }) {
    switch (name) {
      case "MarionetteFrameChild:PageLoadEvent":
        this.emit("page-load-event", data);
        break;
    }
  }
}
