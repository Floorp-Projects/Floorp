/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebNavigation"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// TODO:
// Transition types and qualifiers
// onReferenceFragmentUpdated also triggers for pushState
// getFrames, getAllFrames
// onCreatedNavigationTarget, onHistoryStateUpdated

var Manager = {
  listeners: new Map(),

  init() {
    Services.mm.addMessageListener("Extension:DOMContentLoaded", this);
    Services.mm.addMessageListener("Extension:StateChange", this);
    Services.mm.addMessageListener("Extension:LocationChange", this);
    Services.mm.loadFrameScript("resource://gre/modules/WebNavigationContent.js", true);
  },

  uninit() {
    Services.mm.removeMessageListener("Extension:StateChange", this);
    Services.mm.removeMessageListener("Extension:LocationChange", this);
    Services.mm.removeMessageListener("Extension:DOMContentLoaded", this);
    Services.mm.removeDelayedFrameScript("resource://gre/modules/WebNavigationContent.js");
    Services.mm.broadcastAsyncMessage("Extension:DisableWebNavigation");
  },

  addListener(type, listener) {
    if (this.listeners.size == 0) {
      this.init();
    }

    if (!this.listeners.has(type)) {
      this.listeners.set(type, new Set());
    }
    let listeners = this.listeners.get(type);
    listeners.add(listener);
  },

  removeListener(type, listener) {
    let listeners = this.listeners.get(type);
    if (!listeners) {
      return;
    }
    listeners.delete(listener);
    if (listeners.size == 0) {
      this.listeners.delete(type);
    }

    if (this.listeners.size == 0) {
      this.uninit();
    }
  },

  receiveMessage({name, data, target}) {
    switch (name) {
      case "Extension:StateChange":
        this.onStateChange(target, data);
        break;

      case "Extension:LocationChange":
        this.onLocationChange(target, data);
        break;

      case "Extension:DOMContentLoaded":
        this.onLoad(target, data);
        break;
    }
  },

  onStateChange(browser, data) {
    let stateFlags = data.stateFlags;
    if (stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
      let url = data.requestURL;
      if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
        this.fire("onBeforeNavigate", browser, data, {url});
      } else if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        if (Components.isSuccessCode(data.status)) {
          this.fire("onCompleted", browser, data, {url});
        } else {
          let error = `Error code ${data.status}`;
          this.fire("onErrorOccurred", browser, data, {error, url});
        }
      }
    }
  },

  onLocationChange(browser, data) {
    let url = data.location;
    if (data.flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
      this.fire("onReferenceFragmentUpdated", browser, data, {url});
    } else {
      this.fire("onCommitted", browser, data, {url});
    }
  },

  onLoad(browser, data) {
    this.fire("onDOMContentLoaded", browser, data, {url: data.url});
  },

  fire(type, browser, data, extra) {
    let listeners = this.listeners.get(type);
    if (!listeners) {
      return;
    }

    let details = {
      browser,
      windowId: data.windowId,
    };

    if (data.parentWindowId) {
      details.parentWindowId = data.parentWindowId;
    }

    for (let prop in extra) {
      details[prop] = extra[prop];
    }

    for (let listener of listeners) {
      listener(details);
    }
  },
};

const EVENTS = [
  "onBeforeNavigate",
  "onCommitted",
  "onDOMContentLoaded",
  "onCompleted",
  "onErrorOccurred",
  "onReferenceFragmentUpdated",

  // "onCreatedNavigationTarget",
  // "onHistoryStateUpdated",
];

var WebNavigation = {};

for (let event of EVENTS) {
  WebNavigation[event] = {
    addListener: Manager.addListener.bind(Manager, event),
    removeListener: Manager.removeListener.bind(Manager, event),
  };
}
