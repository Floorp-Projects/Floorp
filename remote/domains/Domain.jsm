/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Domain"];

class Domain {
  constructor(session) {
    this.session = session;
    this.name = this.constructor.name;

    this.eventListeners_ = new Set();
  }

  destructor() {}

  emit(eventName, params = {}) {
    for (const listener of this.eventListeners_) {
      try {
        if (isEventHandler(listener)) {
          listener.onEvent(eventName, params);
        } else {
          listener.call(this, eventName, params);
        }
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }

  addEventListener(listener) {
    if (typeof listener != "function" && !isEventHandler(listener)) {
      throw new TypeError();
    }
    this.eventListeners_.add(listener);
  }

  // static

  static implements(command) {
    return command && typeof this.prototype[command] == "function";
  }
}

function isEventHandler(listener) {
  return listener &&
      "onEvent" in listener &&
      typeof listener.onEvent == "function";
}
