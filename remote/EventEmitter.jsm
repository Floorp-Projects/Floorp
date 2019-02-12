/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["EventEmitter"];

const LISTENERS = Symbol("EventEmitter/listeners");
const ONCE_ORIGINAL_LISTENER = Symbol("EventEmitter/once-original-listener");

const BAD_LISTENER = "Listener must be a function " +
    "or an object that has an onevent function";

class EventEmitter {
  constructor() {
    this.listeners = new Map();
  }

  static on(target, type, listener) {
    if (typeof listener != "function" && !isEventHandler(listener)) {
      throw new TypeError(BAD_LISTENER);
    }

    if (!(LISTENERS in target)) {
      target[LISTENERS] = new Map();
    }

    const events = target[LISTENERS];
    if (events.has(type)) {
      events.get(type).add(listener);
    } else {
      events.set(type, new Set([listener]));
    }
  }

  static off(target, type = undefined, listener = undefined) {
    if (!target[LISTENERS]) {
      return;
    }

    if (listener) {
      EventEmitter._removeListener(target, type, listener);
    } else if (type) {
      EventEmitter._removeTypedListeners(target, type);
    } else {
      EventEmitter._removeAllListeners(target);
    }
  }

  static _removeListener(target, type, listener) {
    const events = target[LISTENERS];

    const listenersForType = events.get(type);
    if (!listenersForType) {
      return;
    }

    if (listenersForType.has(listener)) {
      listenersForType.delete(listener);
    } else {
      for (const value of listenersForType.values()) {
        if (ONCE_ORIGINAL_LISTENER in value &&
            value[ONCE_ORIGINAL_LISTENER] === listener) {
          listenersForType.delete(value);
          break;
        }
      }
    }
  }

  static _removeTypedListeners(target, type) {
    const events = target[LISTENERS];
    if (events.has(type)) {
      events.delete(type);
    }
  }

  static _removeAllListeners(target) {
    const events = target[LISTENERS];
    events.clear();
  }

  static once(target, type, listener) {
    return new Promise(resolve => {
      const newListener = (first, ...rest) => {
        EventEmitter.off(target, type, listener);
        invoke(listener, target, type, first, ...rest);
        resolve(first);
      };

      newListener[ONCE_ORIGINAL_LISTENER] = listener;
      EventEmitter.on(target, type, newListener);
    });
  }

  static emit(target, type, ...rest) {
    if (!(LISTENERS in target)) {
      return;
    }

    for (const [candidate, listeners] of target[LISTENERS]) {
      if (!type.match(expr(candidate))) {
        continue;
      }

      for (const listener of listeners) {
        invoke(listener, target, type, ...rest);
      }
    }
  }

  static decorate(target) {
    const descriptors = Object.getOwnPropertyDescriptors(this.prototype);
    delete descriptors.constructor;
    return Object.defineProperties(target, descriptors);
  }

  on(...args) {
    EventEmitter.on(this, ...args);
  }

  off(...args) {
    EventEmitter.off(this, ...args);
  }

  once(...args) {
    EventEmitter.once(this, ...args);
  }

  emit(...args) {
    EventEmitter.emit(this, ...args);
  }
}

function invoke(listener, target, type, ...args) {
  if (!listener) {
    return;
  }

  try {
    if (isEventHandler(listener)) {
      listener.onevent(type, ...args);
    } else {
      listener.call(target, ...args);
    }
  } catch (e) {
    Cu.reportError(e);
  }
}

function isEventHandler(listener) {
  return listener && "onevent" in listener &&
      typeof listener.onevent == "function";
}

function expr(type) {
  return new RegExp(type.replace("*", "([a-zA-Z.:-]+)"));
}
