/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

("use strict");

const EXPORTED_SYMBOLS = ["EventDispatcher", "MarionetteEventsParent"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
});

// Singleton to allow forwarding events to registered listeners.
const EventDispatcher = {
  init() {
    EventEmitter.decorate(this);
  },
};
EventDispatcher.init();

class MarionetteEventsParent extends JSWindowActorParent {
  async receiveMessage(msg) {
    const { name, data } = msg;

    let rv;
    switch (name) {
      case "MarionetteEventsChild:PageLoadEvent":
        EventDispatcher.emit("page-load", data);
        break;
    }

    return rv;
  }
}
