/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["EventSource"];

Components.utils.import("resource://services-common/utils.js");

var EventSource = function(types, suspendFunc, resumeFunc) {
  this.listeners = new Map();
  for (let type of types) {
    this.listeners.set(type, new Set());
  }

  this.suspend = suspendFunc || function() {};
  this.resume = resumeFunc || function() {};

  this.addEventListener = this.addEventListener.bind(this);
  this.removeEventListener = this.removeEventListener.bind(this);
};

EventSource.prototype = {
  addEventListener(type, listener) {
    if (!this.listeners.has(type)) {
      return;
    }
    this.listeners.get(type).add(listener);
    this.resume();
  },

  removeEventListener(type, listener) {
    if (!this.listeners.has(type)) {
      return;
    }
    this.listeners.get(type).delete(listener);
    if (!this.hasListeners()) {
      this.suspend();
    }
  },

  hasListeners() {
    for (let l of this.listeners.values()) {
      if (l.size > 0) {
        return true;
      }
    }
    return false;
  },

  emit(type, arg) {
    if (!this.listeners.has(type)) {
      return;
    }
    CommonUtils.nextTick(
      function() {
        for (let listener of this.listeners.get(type)) {
          listener(arg);
        }
      },
      this
    );
  },
};

this.EventSource = EventSource;
