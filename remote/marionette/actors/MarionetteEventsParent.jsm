/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

("use strict");

const EXPORTED_SYMBOLS = [
  "disableEventsActor",
  "enableEventsActor",
  "EventDispatcher",
  "MarionetteEventsParent",
];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

// Singleton to allow forwarding events to registered listeners.
const EventDispatcher = {
  init() {
    lazy.EventEmitter.decorate(this);
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

// Flag to check if the MarionetteEvents actors have already been registed.
let eventsActorRegistered = false;

/**
 * Register Events actors to listen for page load events via EventDispatcher.
 */
function registerEventsActor() {
  if (eventsActorRegistered) {
    return;
  }

  try {
    // Register the JSWindowActor pair for events as used by Marionette
    ChromeUtils.registerWindowActor("MarionetteEvents", {
      kind: "JSWindowActor",
      parent: {
        moduleURI:
          "chrome://remote/content/marionette/actors/MarionetteEventsParent.jsm",
      },
      child: {
        moduleURI:
          "chrome://remote/content/marionette/actors/MarionetteEventsChild.jsm",
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

    eventsActorRegistered = true;
  } catch (e) {
    if (e.name === "NotSupportedError") {
      lazy.logger.warn(`MarionetteEvents actor is already registered!`);
    } else {
      throw e;
    }
  }
}

/**
 * Enable MarionetteEvents actors to start forwarding page load events from the
 * child actor to the parent actor. Register the MarionetteEvents actor if necessary.
 */
function enableEventsActor() {
  // sharedData is replicated across processes and will be checked by
  // MarionetteEventsChild before forward events to the parent actor.
  Services.ppmm.sharedData.set("MARIONETTE_EVENTS_ENABLED", true);
  // Request to immediately flush the data to the content processes to avoid races.
  Services.ppmm.sharedData.flush();

  registerEventsActor();
}

/**
 * Disable MarionetteEvents actors to stop forwarding page load events from the
 * child actor to the parent actor.
 */
function disableEventsActor() {
  Services.ppmm.sharedData.set("MARIONETTE_EVENTS_ENABLED", false);
  Services.ppmm.sharedData.flush();
}
