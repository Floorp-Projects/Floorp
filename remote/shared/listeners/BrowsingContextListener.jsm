/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["BrowsingContextListener"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
});

const OBSERVER_TOPIC_ATTACHED = "browsing-context-attached";
const OBSERVER_TOPIC_DISCARDED = "browsing-context-discarded";

const OBSERVER_TOPIC_SET_EMBEDDER = "browsing-context-did-set-embedder";

/**
 * The BrowsingContextListener can be used to listen for notifications coming
 * from browsing contexts that get attached or discarded.
 *
 * Example:
 * ```
 * const listener = new BrowsingContextListener();
 * listener.on("attached", onAttached);
 * listener.startListening();
 *
 * const onAttached = (eventName, data = {}) => {
 *   const { browsingContext, why } = data;
 *   ...
 * };
 * ```
 *
 * @emits message
 *    The BrowsingContextListener emits "attached" and "discarded" events,
 *    with the following object as payload:
 *      - {BrowsingContext} browsingContext
 *            Browsing context the notification relates to.
 *      - {string} why
 *            Usually "attach" or "discard", but will contain "replace" if the
 *            browsing context gets replaced by a cross-group navigation.
 */
class BrowsingContextListener {
  #listening;
  #topContextsToAttach;

  /**
   * Create a new BrowsingContextListener instance.
   */
  constructor() {
    lazy.EventEmitter.decorate(this);

    // A map that temporarily holds attached top-level browsing contexts until
    // their embedder element is set, which is required to successfully
    // retrieve a unique id for the content browser by the TabManager.
    this.#topContextsToAttach = new Map();

    this.#listening = false;
  }

  destroy() {
    this.stopListening();
  }

  observe(subject, topic, data) {
    switch (topic) {
      case OBSERVER_TOPIC_ATTACHED:
        // Delay emitting the event for top-level browsing contexts until
        // the embedder element has been set.
        if (!subject.parent) {
          this.#topContextsToAttach.set(subject, data);
          return;
        }

        this.emit("attached", { browsingContext: subject, why: data });
        break;

      case OBSERVER_TOPIC_DISCARDED:
        // Remove a recently attached top-level browsing context if it's
        // immediately discarded.
        if (this.#topContextsToAttach.has(subject)) {
          this.#topContextsToAttach.delete(subject);
        }

        this.emit("discarded", { browsingContext: subject, why: data });
        break;

      case OBSERVER_TOPIC_SET_EMBEDDER:
        const why = this.#topContextsToAttach.get(subject);
        if (why !== undefined) {
          this.emit("attached", { browsingContext: subject, why });
          this.#topContextsToAttach.delete(subject);
        }
        break;
    }
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    Services.obs.addObserver(this, OBSERVER_TOPIC_ATTACHED);
    Services.obs.addObserver(this, OBSERVER_TOPIC_DISCARDED);
    Services.obs.addObserver(this, OBSERVER_TOPIC_SET_EMBEDDER);

    this.#listening = true;
  }

  stopListening() {
    if (!this.#listening) {
      return;
    }

    Services.obs.removeObserver(this, OBSERVER_TOPIC_ATTACHED);
    Services.obs.removeObserver(this, OBSERVER_TOPIC_DISCARDED);
    Services.obs.removeObserver(this, OBSERVER_TOPIC_SET_EMBEDDER);

    this.#topContextsToAttach.clear();

    this.#listening = false;
  }
}
