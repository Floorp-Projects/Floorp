/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["BrowsingContextListener"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const OBSERVER_TOPIC_ATTACHED = "browsing-context-attached";
const OBSERVER_TOPIC_DISCARDED = "browsing-context-discarded";

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

  /**
   * Create a new BrowsingContextListener instance.
   */
  constructor() {
    EventEmitter.decorate(this);

    this.#listening = false;
  }

  destroy() {
    this.stopListening();
  }

  observe(subject, topic, data) {
    switch (topic) {
      case OBSERVER_TOPIC_ATTACHED:
        this.emit("attached", { browsingContext: subject, why: data });
        break;
      case OBSERVER_TOPIC_DISCARDED:
        this.emit("discarded", { browsingContext: subject, why: data });
        break;
    }
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    Services.obs.addObserver(this, OBSERVER_TOPIC_ATTACHED);
    Services.obs.addObserver(this, OBSERVER_TOPIC_DISCARDED);

    this.#listening = true;
  }

  stopListening() {
    if (!this.#listening) {
      return;
    }

    Services.obs.removeObserver(this, OBSERVER_TOPIC_ATTACHED);
    Services.obs.removeObserver(this, OBSERVER_TOPIC_DISCARDED);

    this.#listening = false;
  }
}
