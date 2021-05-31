/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

("use strict");

const EXPORTED_SYMBOLS = [
  "EventDispatcher",
  "MarionetteEventsParent",
  "registerEventsActor",
  "unregisterEventsActor",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
  Log: "chrome://marionette/content/log.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

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

/**
 * Register Events actors to listen for page load events via EventDispatcher.
 */
function registerEventsActor() {
  try {
    // Register the JSWindowActor pair for events as used by Marionette
    ChromeUtils.registerWindowActor("MarionetteEvents", {
      kind: "JSWindowActor",
      parent: {
        moduleURI:
          "chrome://marionette/content/actors/MarionetteEventsParent.jsm",
      },
      child: {
        moduleURI:
          "chrome://marionette/content/actors/MarionetteEventsChild.jsm",
        events: {
          beforeunload: { capture: true },
          DOMContentLoaded: { mozSystemGroup: true },
          hashchange: { mozSystemGroup: true },
          pagehide: { mozSystemGroup: true },
          pageshow: { mozSystemGroup: true },
          // popstate doesn't bubble, as such use capturing phase
          popstate: { capture: true, mozSystemGroup: true },

          click: {},
          dblclick: {},
          unload: { capture: true, createActor: false },
        },
      },

      allFrames: true,
      includeChrome: true,
    });
  } catch (e) {
    if (e.name === "NotSupportedError") {
      logger.warn(`MarionetteEvents actor is already registered!`);
    } else {
      throw e;
    }
  }
}

function unregisterEventsActor() {
  ChromeUtils.unregisterWindowActor("MarionetteEvents");
}
