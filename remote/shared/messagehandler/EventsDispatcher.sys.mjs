/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  SessionDataCategory:
    "chrome://remote/content/shared/messagehandler/sessiondata/SessionData.sys.mjs",
  SessionDataMethod:
    "chrome://remote/content/shared/messagehandler/sessiondata/SessionData.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

/**
 * Helper to listen to events which rely on SessionData.
 * In order to support the EventsDispatcher, a module emitting events should
 * subscribe and unsubscribe to those events based on SessionData updates
 * and should use the "event" SessionData category.
 */
export class EventsDispatcher {
  // The MessageHandler owning this EventsDispatcher.
  #messageHandler;

  /**
   * @typedef {object} EventListenerInfo
   * @property {ContextDescriptor} contextDescriptor
   *     The ContextDescriptor to which those callbacks are associated
   * @property {Set<Function>} callbacks
   *     The callbacks to trigger when an event matching the ContextDescriptor
   *     is received.
   */

  // Map from event name to map of strings (context keys) to EventListenerInfo.
  #listenersByEventName;

  /**
   * Create a new EventsDispatcher instance.
   *
   * @param {MessageHandler} messageHandler
   *     The MessageHandler owning this EventsDispatcher.
   */
  constructor(messageHandler) {
    this.#messageHandler = messageHandler;

    this.#listenersByEventName = new Map();
  }

  destroy() {
    for (const event of this.#listenersByEventName.keys()) {
      this.#messageHandler.off(event, this.#onMessageHandlerEvent);
    }

    this.#listenersByEventName = null;
  }

  /**
   * Check for existing listeners for a given event name and a given context.
   *
   * @param {string} name
   *     Name of the event to check.
   * @param {ContextInfo} contextInfo
   *     ContextInfo identifying the context to check.
   *
   * @returns {boolean}
   *     True if there is a registered listener matching the provided arguments.
   */
  hasListener(name, contextInfo) {
    if (!this.#listenersByEventName.has(name)) {
      return false;
    }

    const listeners = this.#listenersByEventName.get(name);
    for (const { contextDescriptor } of listeners.values()) {
      if (this.#matchesContext(contextInfo, contextDescriptor)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Stop listening for an event relying on SessionData and relayed by the
   * message handler.
   *
   * @param {string} event
   *     Name of the event to unsubscribe from.
   * @param {ContextDescriptor} contextDescriptor
   *     Context descriptor for this event.
   * @param {Function} callback
   *     Event listener callback.
   * @returns {Promise}
   *     Promise which resolves when the event fully unsubscribed, including
   *     propagating the necessary session data.
   */
  async off(event, contextDescriptor, callback) {
    return this.update([{ event, contextDescriptor, callback, enable: false }]);
  }

  /**
   * Listen for an event relying on SessionData and relayed by the message
   * handler.
   *
   * @param {string} event
   *     Name of the event to subscribe to.
   * @param {ContextDescriptor} contextDescriptor
   *     Context descriptor for this event.
   * @param {Function} callback
   *     Event listener callback.
   * @returns {Promise}
   *     Promise which resolves when the event fully subscribed to, including
   *     propagating the necessary session data.
   */
  async on(event, contextDescriptor, callback) {
    return this.update([{ event, contextDescriptor, callback, enable: true }]);
  }

  /**
   * An object that holds information about subscription/unsubscription
   * of an event.
   *
   * @typedef Subscription
   *
   * @param {string} event
   *     Name of the event to subscribe/unsubscribe to.
   * @param {ContextDescriptor} contextDescriptor
   *     Context descriptor for this event.
   * @param {Function} callback
   *     Event listener callback.
   * @param {boolean} enable
   *     True, if we need to subscribe to an event.
   *     Otherwise false.
   */

  /**
   * Start or stop listening to a list of events relying on SessionData
   * and relayed by the message handler.
   *
   * @param {Array<Subscription>} subscriptions
   *     The list of information to subscribe/unsubscribe to.
   *
   * @returns {Promise}
   *     Promise which resolves when the events fully subscribed/unsubscribed to,
   *     including propagating the necessary session data.
   */
  async update(subscriptions) {
    const sessionDataItemUpdates = [];
    subscriptions.forEach(({ event, contextDescriptor, callback, enable }) => {
      if (enable) {
        // Setup listeners.
        if (!this.#listenersByEventName.has(event)) {
          this.#listenersByEventName.set(event, new Map());
          this.#messageHandler.on(event, this.#onMessageHandlerEvent);
        }

        const key = this.#getContextKey(contextDescriptor);
        const listeners = this.#listenersByEventName.get(event);
        if (listeners.has(key)) {
          const { callbacks } = listeners.get(key);
          callbacks.add(callback);
        } else {
          const callbacks = new Set([callback]);
          listeners.set(key, { callbacks, contextDescriptor });

          sessionDataItemUpdates.push({
            ...this.#getSessionDataItem(event, contextDescriptor),
            method: lazy.SessionDataMethod.Add,
          });
        }
      } else {
        // Remove listeners.
        const listeners = this.#listenersByEventName.get(event);
        if (!listeners) {
          return;
        }

        const key = this.#getContextKey(contextDescriptor);
        if (!listeners.has(key)) {
          return;
        }

        const { callbacks } = listeners.get(key);
        if (callbacks.has(callback)) {
          callbacks.delete(callback);
          if (callbacks.size === 0) {
            listeners.delete(key);
            if (listeners.size === 0) {
              this.#messageHandler.off(event, this.#onMessageHandlerEvent);
              this.#listenersByEventName.delete(event);
            }

            sessionDataItemUpdates.push({
              ...this.#getSessionDataItem(event, contextDescriptor),
              method: lazy.SessionDataMethod.Remove,
            });
          }
        }
      }
    });

    // Update all sessionData at once.
    await this.#messageHandler.updateSessionData(sessionDataItemUpdates);
  }

  #getContextKey(contextDescriptor) {
    const { id, type } = contextDescriptor;
    return `${type}-${id}`;
  }

  #getSessionDataItem(event, contextDescriptor) {
    const [moduleName] = event.split(".");
    return {
      moduleName,
      category: lazy.SessionDataCategory.Event,
      contextDescriptor,
      values: [event],
    };
  }

  #matchesContext(contextInfo, contextDescriptor) {
    if (contextDescriptor.type === lazy.ContextDescriptorType.All) {
      return true;
    }

    if (
      contextDescriptor.type === lazy.ContextDescriptorType.TopBrowsingContext
    ) {
      const eventBrowsingContext = lazy.TabManager.getBrowsingContextById(
        contextInfo.contextId
      );
      return eventBrowsingContext?.browserId === contextDescriptor.id;
    }

    return false;
  }

  #onMessageHandlerEvent = (name, event, contextInfo) => {
    const listeners = this.#listenersByEventName.get(name);
    for (const { callbacks, contextDescriptor } of listeners.values()) {
      if (!this.#matchesContext(contextInfo, contextDescriptor)) {
        continue;
      }

      for (const callback of callbacks) {
        try {
          callback(name, event);
        } catch (e) {
          lazy.logger.debug(
            `Error while executing callback for ${name}: ${e.message}`
          );
        }
      }
    }
  };
}
