/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["EventsDispatcher"];

/**
 * Helper to listen to internal events which rely on SessionData.
 * In order to support the EventsDispatcher, a module emitting internal events
 * should subscribe and unsubscribe to those events based on SessionData updates
 * and should use the "internal-event" SessionData category.
 */
class EventsDispatcher {
  // The MessageHandler owning this EventsDispatcher.
  #messageHandler;

  // Map from event name to event listener callbacks map.
  // A listener callbacks map is a map from a string context id (eg browser id,
  // may depend on the event) to a Set of functions.
  #eventListeners;

  /**
   * Create a new EventsDispatcher instance.
   *
   * @param {MessageHandler} messageHandler
   *     The MessageHandler owning this EventsDispatcher.
   */
  constructor(messageHandler) {
    this.#messageHandler = messageHandler;

    this.#eventListeners = new Map();
  }

  destroy() {
    this.#eventListeners.clear();
  }

  /**
   * Stop listening for an internal event relying on SessionData and relayed by
   * the message handler.
   *
   * @param {string} event
   *     Name of the internal event to unsubscribe from.
   * @param {ContextDescriptor} contextDescriptor
   *     Context descriptor for this event.
   * @param {function} callback
   *     Event listener callback.
   * @return {Promise}
   *     Promise which resolves when the event fully unsubscribed, including
   *     propagating the necessary session data.
   */
  async off(event, contextDescriptor, callback) {
    const listeners = this.#eventListeners.get(event);
    if (!listeners) {
      return;
    }

    const key = this.#getContextKey(contextDescriptor);
    const callbacks = listeners.get(key);
    if (!callbacks) {
      return;
    }

    if (callbacks.has(callback)) {
      callbacks.delete(callback);
      if (callbacks.size === 0) {
        await this.#messageHandler.removeSessionData(
          this.#getSessionDataItem(event, contextDescriptor)
        );
        listeners.delete(key);
      }
    }

    this.#messageHandler.off(event, callback);
  }

  /**
   * Listen for an internal event relying on SessionData and relayed by the
   * message handler.
   *
   * @param {string} event
   *     Name of the internal event to subscribe to.
   * @param {ContextDescriptor} contextDescriptor
   *     Context descriptor for this event.
   * @param {function} callback
   *     Event listener callback.
   * @return {Promise}
   *     Promise which resolves when the event fully subscribed to, including
   *     propagating the necessary session data.
   */
  async on(event, contextDescriptor, callback) {
    if (!this.#eventListeners.has(event)) {
      this.#eventListeners.set(event, new Map());
    }

    const key = this.#getContextKey(contextDescriptor);
    const listeners = this.#eventListeners.get(event);
    if (listeners.has(key)) {
      const callbacks = listeners.get(key);
      callbacks.add(callback);
    } else {
      listeners.set(key, new Set([callback]));
      await this.#messageHandler.addSessionData(
        this.#getSessionDataItem(event, contextDescriptor)
      );
    }

    this.#messageHandler.on(event, callback);
  }

  #getContextKey(contextDescriptor) {
    const { id, type } = contextDescriptor;
    return `${type}-${id}`;
  }

  #getSessionDataItem(event, contextDescriptor) {
    const [moduleName] = event.split(".");
    return {
      moduleName,
      category: "internal-event",
      contextDescriptor,
      values: [event],
    };
  }
}
