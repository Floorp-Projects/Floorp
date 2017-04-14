/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Adapters"];

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/CloudSyncEventSource.jsm");

this.Adapters = function() {
  let eventTypes = [
    "sync",
  ];

  let suspended = true;

  let suspend = function() {
    if (!suspended) {
      Services.obs.removeObserver(observer, "cloudsync:user-sync");
      suspended = true;
    }
  };

  let resume = function() {
    if (suspended) {
      Services.obs.addObserver(observer, "cloudsync:user-sync");
      suspended = false;
    }
  };

  let eventSource = new EventSource(eventTypes, suspend, resume);
  let registeredAdapters = new Map();

  function register(name, opts) {
    opts = opts || {};
    registeredAdapters.set(name, opts);
  }

  function unregister(name) {
    if (!registeredAdapters.has(name)) {
      throw new Error("adapter is not registered: " + name)
    }
    registeredAdapters.delete(name);
  }

  function getAdapterNames() {
    let result = [];
    for (let name of registeredAdapters.keys()) {
      result.push(name);
    }
    return result;
  }

  function getAdapter(name) {
    if (!registeredAdapters.has(name)) {
      throw new Error("adapter is not registered: " + name)
    }
    return registeredAdapters.get(name);
  }

  function countAdapters() {
    return registeredAdapters.size;
  }

  let observer = {
    observe(subject, topic, data) {
      switch (topic) {
        case "cloudsync:user-sync":
          eventSource.emit("sync");
          break;
      }
    }
  };

  this.addEventListener = eventSource.addEventListener;
  this.removeEventListener = eventSource.removeEventListener;
  this.register = register.bind(this);
  this.get = getAdapter.bind(this);
  this.unregister = unregister.bind(this);
  this.__defineGetter__("names", getAdapterNames);
  this.__defineGetter__("count", countAdapters);
};

Adapters.prototype = {

};
