/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Memory",
  "resource://gre/modules/Memory.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

// Helper class that handles observing a topic and forwarding the data payload
// to a set of registered listeners.
class ListenerRelay {
  /**
   * @param {string} topic   The topic that will be observed.
   **/
  constructor(topic) {
    this.listeners = new Set();
    this.topic = topic;
  }

  // Removes ourselves from the observer service
  // and clears out all registered listeners.
  clear() {
    if (this.listeners.size) {
      Services.obs.removeObserver(this, this.topic);
      this.listeners.clear();
    }
  }

  /**
   * Adds a callback to invoke when our topic is observed.
   * @param {function} callback   The callback to invoke.
   **/
  add(callback) {
    if (!this.listeners.size) {
      Services.obs.addObserver(this, this.topic);
    }

    this.listeners.add(callback);
  }

  /**
   * Removes a callback that was previously registered.
   * @param {function} callback   The callback to remove.
   **/
  remove(callback) {
    this.listeners.delete(callback);

    if (!this.listeners.size) {
      Services.obs.removeObserver(this, this.topic);
    }
  }

  /**
   * Callback function invoked by the observer service when our topic is hit.
   * @param {null}   subject  Not being used by this function
   * @param {string} topic    The topic being observed. This should only be
   *                          `this.topic`.
   * @param {string} data     The data payload that will be forwarded to the
   *                          listeners.
   **/
  observe(subject, topic, data) {
    if (topic !== this.topic) {
      return;
    }

    // Forward to our set of listeners.
    for (let listener of this.listeners) {
      try {
        listener(data);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }
}

// Relay used for memory-pressure events.
XPCOMUtils.defineLazyGetter(this, "lowMemListeners", () => {
  return new ListenerRelay("memory-pressure");
});

// The browser.memory API.
this.memory = class extends ExtensionAPI {
  getAPI(context) {
    return {
      memory: {
        // Super basic, this gives back USS and RSS of each process.
        // Other interesting things to add:
        //   - Ghost windows
        //   - Available memory (chrome does this)
        //   - Capacity (I guess physical memory, chrome does this)
        //   - VSS
        async getInfo() {
          return Memory.summary();
        },

        // Provides the same functionality as about:memory's "minimize memory
        // usage" button.
        async minimizeMemoryUsage() {
          await new Promise(resolve => {
            const mgr = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
              Ci.nsIMemoryReporterManager
            );
            mgr.minimizeMemoryUsage(resolve);
          });
        },

        // Event that is fired when a low memory signal is recieved.
        onLowMemory: new ExtensionCommon.EventManager({
          context,
          name: "memory.onLowMemory",
          register: fire => {
            // Register the listener with our relay.
            lowMemListeners.add(fire.async);
            return () => {
              lowMemListeners.remove(fire.async);
            };
          },
        }).api(),
      },
    };
  }
};
