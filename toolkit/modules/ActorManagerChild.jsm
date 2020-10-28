/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This module implements logic for creating JavaScript IPC actors, as defined
 * in ActorManagerParent, for frame message manager contexts. See
 * ActorManagerParent.jsm for more information.
 */

var EXPORTED_SYMBOLS = ["ActorManagerChild"];

const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "WebNavigationFrames",
  "resource://gre/modules/WebNavigationFrames.jsm"
);

const { DefaultMap } = ExtensionUtils;

const { sharedData } = Services.cpmm;

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "simulateEvents",
  "fission.frontend.simulate-events",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "simulateMessages",
  "fission.frontend.simulate-messages",
  false
);

function getMessageManager(window) {
  return window.docShell.messageManager;
}

class Dispatcher {
  constructor(mm, data) {
    this.mm = mm;

    this.actors = data.actors;
    this.events = data.events;
    this.messages = data.messages;
    this.observers = data.observers;

    this.instances = new Map();
  }

  init() {
    for (let msg of this.messages.keys()) {
      // This is directly called on the message manager
      // because this.addMessageListener is meant to handle
      // additions after initialization.
      this.mm.addMessageListener(msg, this);
    }
    for (let topic of this.observers.keys()) {
      Services.obs.addObserver(this, topic, true);
    }
    for (let { event, options, actor } of this.events) {
      this.addEventListener(event, actor, options);
    }

    this.mm.addEventListener("unload", this);
  }

  cleanup() {
    for (let topic of this.observers.keys()) {
      Services.obs.removeObserver(this, topic);
    }

    this.mm.removeEventListener("unload", this);
  }

  get window() {
    return this.mm.content;
  }

  get frameId() {
    // 0 for top-level windows, BrowsingContext ID otherwise
    return WebNavigationFrames.getFrameId(this.window);
  }

  get browsingContextId() {
    return this.window.docShell.browsingContext.id;
  }

  addEventListener(event, actor, options) {
    let listener = this.handleActorEvent.bind(this, actor);
    this.mm.addEventListener(event, listener, options);
  }

  addMessageListener(msg, actor) {
    let actors = this.messages.get(msg);

    if (!actors) {
      actors = [];
      this.messages.set(msg, actors);
    }

    if (!actors.length) {
      this.mm.addMessageListener(msg, this);
    }

    if (!actors.includes(actor)) {
      actors.push(actor);
    }
  }

  getActor(actorName) {
    let inst = this.instances.get(actorName);
    if (!inst) {
      let actor = this.actors.get(actorName);

      let obj = {};
      ChromeUtils.import(actor.module, obj);

      inst = new obj[actorName](this);
      this.instances.set(actorName, inst);
    }
    return inst;
  }

  handleEvent(event) {
    if (event.type == "unload") {
      this.cleanup();
    }
  }

  handleActorEvent(actor, event) {
    let targetWindow = null;

    if (simulateEvents) {
      targetWindow = event.target.ownerGlobal;
      if (targetWindow != this.window) {
        // events can't propagate across frame boundaries because the
        // frames will be hosted on separated processes.
        return;
      }
    }
    this.getActor(actor).handleEvent(event);
  }

  receiveMessage(message) {
    let actors = this.messages.get(message.name);

    if (simulateMessages) {
      let match = false;
      let data = message.data || {};
      if (data.hasOwnProperty("frameId")) {
        match = data.frameId == this.frameId;
      } else if (data.hasOwnProperty("browsingContextId")) {
        match = data.browsingContextId == this.browsingContextId;
      } else {
        // if no specific target was given, just dispatch it to
        // top-level actors.
        match = this.frameId == 0;
      }

      if (!match) {
        return;
      }
    }

    for (let actor of actors) {
      try {
        this.getActor(actor).receiveMessage(message);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }

  observe(subject, topic, data) {
    let actors = this.observers.get(topic);
    for (let actor of actors) {
      try {
        this.getActor(actor).observe(subject, topic, data);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }
}

Dispatcher.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIObserver",
  "nsISupportsWeakReference",
]);

class SingletonDispatcher extends Dispatcher {
  constructor(window, data) {
    super(getMessageManager(window), data);

    window.addEventListener("pageshow", this, { mozSystemGroup: true });
    window.addEventListener("pagehide", this, { mozSystemGroup: true });

    this._window = Cu.getWeakReference(window);
    this.listeners = [];
  }

  init() {
    super.init();

    for (let actor of this.instances.values()) {
      if (typeof actor.init === "function") {
        try {
          actor.init();
        } catch (e) {
          Cu.reportError(e);
        }
      }
    }
  }

  cleanup() {
    super.cleanup();

    for (let msg of this.messages.keys()) {
      this.mm.removeMessageListener(msg, this);
    }
    for (let [event, listener, options] of this.listeners) {
      this.mm.removeEventListener(event, listener, options);
    }

    for (let actor of this.instances.values()) {
      if (typeof actor.cleanup == "function") {
        try {
          actor.cleanup();
        } catch (e) {
          Cu.reportError(e);
        }
      }
    }

    this.listeners = [];
  }

  get window() {
    return this._window.get();
  }

  handleEvent(event) {
    if (event.type == "pageshow") {
      if (this.hidden) {
        this.init();
      }
      this.hidden = false;
    } else if (event.type == "pagehide") {
      this.hidden = true;
      this.cleanup();
    }
  }

  handleActorEvent(actor, event) {
    if (event.target.ownerGlobal == this.window) {
      const inst = this.getActor(actor);
      if (typeof inst.handleEvent != "function") {
        throw new Error(
          `Unhandled event for ${actor}: ${event.type}: missing handleEvent`
        );
      }
      inst.handleEvent(event);
    }
  }

  addEventListener(event, actor, options) {
    let listener = this.handleActorEvent.bind(this, actor);
    this.listeners.push([event, listener, options]);
    this.mm.addEventListener(event, listener, options);
  }
}

/* globals MatchPatternSet, MozDocumentMatcher, MozDocumentObserver */

var ActorManagerChild = {
  groups: new DefaultMap(group => {
    return sharedData.get(`ChildActors:${group || ""}`);
  }),

  singletons: new Map(),

  init() {
    let singletons = sharedData.get("ChildSingletonActors");
    for (let [filter, data] of singletons.entries()) {
      let options = {
        matches: new MatchPatternSet(filter.matches, {
          restrictSchemes: false,
        }),
        allFrames: filter.allFrames,
        matchAboutBlank: filter.matchAboutBlank,
      };

      this.singletons.set(new MozDocumentMatcher(options), data);
    }

    this.observer = new MozDocumentObserver(this);
    this.observer.observe(this.singletons.keys());

    this.init = null;
  },

  /**
   * MozDocumentObserver callbacks. These handle instantiating singleton actors
   * for documents which match their MozDocumentMatcher filters.
   */
  onNewDocument(matcher, window) {
    new SingletonDispatcher(window, this.singletons.get(matcher)).init();
  },
  onPreloadDocument(matcher, loadInfo) {},

  /**
   * Attaches the appropriate set of actors to the given frame message manager.
   *
   * @param {ContentFrameMessageManager} mm
   *        The message manager to which to attach the actors.
   * @param {string} [group]
   *        The messagemanagergroup of the <browser> to which the caller frame
   *        script belongs. This restricts the attached set of actors based on
   *        the "group" that their actor definitions specify.
   */
  attach(mm, group = null) {
    new Dispatcher(mm, this.groups.get(group)).init();
  },

  getActor(mm, actorName) {
    for (let dispatcher of this.dispatchers.get(mm)) {
      let actor = dispatcher.getActor(actorName);
      if (actor) {
        return actor;
      }
    }
    return null;
  },
};

ActorManagerChild.init();
