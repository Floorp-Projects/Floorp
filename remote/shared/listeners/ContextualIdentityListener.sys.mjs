/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
});

const OBSERVER_TOPIC_CREATED = "contextual-identity-created";
const OBSERVER_TOPIC_DELETED = "contextual-identity-deleted";

/**
 * The ContextualIdentityListener can be used to listen for notifications about
 * contextual identities (containers) being created or deleted.
 *
 * Example:
 * ```
 * const listener = new ContextualIdentityListener();
 * listener.on("created", onCreated);
 * listener.startListening();
 *
 * const onCreated = (eventName, data = {}) => {
 *   const { identity } = data;
 *   ...
 * };
 * ```
 *
 * @fires message
 *    The ContextualIdentityListener emits "created" and "deleted" events,
 *    with the following object as payload:
 *      - {object} identity
 *            The contextual identity which was created or deleted.
 */
export class ContextualIdentityListener {
  #listening;

  /**
   * Create a new BrowsingContextListener instance.
   */
  constructor() {
    lazy.EventEmitter.decorate(this);

    this.#listening = false;
  }

  destroy() {
    this.stopListening();
  }

  observe(subject, topic, data) {
    switch (topic) {
      case OBSERVER_TOPIC_CREATED:
        this.emit("created", { identity: subject.wrappedJSObject });
        break;

      case OBSERVER_TOPIC_DELETED:
        this.emit("deleted", { identity: subject.wrappedJSObject });
        break;
    }
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    Services.obs.addObserver(this, OBSERVER_TOPIC_CREATED);
    Services.obs.addObserver(this, OBSERVER_TOPIC_DELETED);

    this.#listening = true;
  }

  stopListening() {
    if (!this.#listening) {
      return;
    }

    Services.obs.removeObserver(this, OBSERVER_TOPIC_CREATED);
    Services.obs.removeObserver(this, OBSERVER_TOPIC_DELETED);

    this.#listening = false;
  }
}
