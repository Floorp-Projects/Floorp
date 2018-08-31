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

ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

const {DefaultMap} = ExtensionUtils;

const {sharedData} = Services.cpmm;

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
      this.mm.addMessageListener(msg, this);
    }
    for (let topic of this.observers.keys()) {
      Services.obs.addObserver(this, topic, true);
    }
    for (let {event, options, actor} of this.events) {
      this.addEventListener(event, this.handleActorEvent.bind(this, actor), options);
    }

    this.mm.addEventListener("unload", this);
  }

  cleanup() {
    for (let topic of this.observers.keys()) {
      Services.obs.removeObserver(this, topic);
    }
  }

  addEventListener(event, listener, options) {
    this.mm.addEventListener(event, listener, options);
  }

  getActor(actorName) {
    let inst = this.instances.get(actorName);
    if (!inst) {
      let actor = this.actors.get(actorName);

      let obj = {};
      ChromeUtils.import(actor.module, obj);

      inst = new obj[actorName](this.mm);
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
    return this.getActor(actor).handleEvent(event);
  }

  receiveMessage(message) {
    let actors = this.messages.get(message.name);
    let result;
    for (let actor of actors) {
      try {
        result = this.getActor(actor).receiveMessage(message);
      } catch (e) {
        Cu.reportError(e);
      }
    }
    return result;
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

Dispatcher.prototype.QueryInterface =
  ChromeUtils.generateQI(["nsIObserver",
                          "nsISupportsWeakReference"]);

class SingletonDispatcher extends Dispatcher {
  constructor(window, data) {
    super(getMessageManager(window), data);

    window.addEventListener("pageshow", this, {mozSystemGroup: true});
    window.addEventListener("pagehide", this, {mozSystemGroup: true});

    this.window = window;
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
      this.window.removeEventListener(event, listener, options);
    }

    for (let actor of this.instances.values()) {
      if (typeof actor.cleanup === "function") {
        try {
          actor.cleanup();
        } catch (e) {
          Cu.reportError(e);
        }
      }
    }

    this.listeners = null;
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

  addEventListener(event, listener, options) {
    this.listeners.push([event, listener, options]);
    this.window.addEventListener(event, listener, options);
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
        matches: new MatchPatternSet(filter.matches, {restrictSchemes: false}),
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
  onPreloadDocument(matcher, loadInfo) {
  },

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
