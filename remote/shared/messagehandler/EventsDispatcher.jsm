/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["EventsDispatcher"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  TabManager: "chrome://remote/content/shared/TabManager.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

/**
 * Helper to listen to events which rely on SessionData.
 * In order to support the EventsDispatcher, a module emitting events should
 * subscribe and unsubscribe to those events based on SessionData updates
 * and should use the "event" SessionData category.
 */
class EventsDispatcher {
  // The MessageHandler owning this EventsDispatcher.
  #messageHandler;

  /**
   * @typedef {Object} EventListenerInfo
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
   * Stop listening for an event relying on SessionData and relayed by the
   * message handler.
   *
   * @param {string} event
   *     Name of the event to unsubscribe from.
   * @param {ContextDescriptor} contextDescriptor
   *     Context descriptor for this event.
   * @param {function} callback
   *     Event listener callback.
   * @return {Promise}
   *     Promise which resolves when the event fully unsubscribed, including
   *     propagating the necessary session data.
   */
  async off(event, contextDescriptor, callback) {
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

        await this.#messageHandler.removeSessionData(
          this.#getSessionDataItem(event, contextDescriptor)
        );
      }
    }
  }

  /**
   * Listen for an event relying on SessionData and relayed by the message
   * handler.
   *
   * @param {string} event
   *     Name of the event to subscribe to.
   * @param {ContextDescriptor} contextDescriptor
   *     Context descriptor for this event.
   * @param {function} callback
   *     Event listener callback.
   * @return {Promise}
   *     Promise which resolves when the event fully subscribed to, including
   *     propagating the necessary session data.
   */
  async on(event, contextDescriptor, callback) {
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
      await this.#messageHandler.addSessionData(
        this.#getSessionDataItem(event, contextDescriptor)
      );
    }
  }

  #getContextKey(contextDescriptor) {
    const { id, type } = contextDescriptor;
    return `${type}-${id}`;
  }

  #getSessionDataItem(event, contextDescriptor) {
    const [moduleName] = event.split(".");
    return {
      moduleName,
      category: "event",
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
