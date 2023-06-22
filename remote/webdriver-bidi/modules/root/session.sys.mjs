/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Marionette: "chrome://remote/content/components/Marionette.sys.mjs",
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
});

class SessionModule extends Module {
  #browsingContextIdEventMap;
  #globalEventSet;

  constructor(messageHandler) {
    super(messageHandler);

    // Map with top-level browsing context id keys and values
    // that are a set of event names for events
    // that are enabled in the given browsing context.
    // TODO: Bug 1804417. Use navigable instead of browsing context id.
    this.#browsingContextIdEventMap = new Map();

    // Set of event names which are strings of the form [moduleName].[eventName]
    // for events that are enabled for all browsing contexts.
    // We should only add an actual event listener on the MessageHandler the
    // first time an event is subscribed to.
    this.#globalEventSet = new Set();
  }

  destroy() {
    this.#browsingContextIdEventMap = null;
    this.#globalEventSet = null;
  }

  /**
   * Commands
   */

  /**
   * End the current session.
   *
   * Session clean up will happen later in WebDriverBiDiConnection class.
   */
  async end() {
    if (lazy.Marionette.running) {
      throw new lazy.error.UnsupportedOperationError(
        "Ending session which was started with Webdriver classic is not supported, use Webdriver classic delete command instead."
      );
    }
  }

  /**
   * Enable certain events either globally, or for a list of browsing contexts.
   *
   * @param {object=} params
   * @param {Array<string>} params.events
   *     List of events to subscribe to.
   * @param {Array<string>=} params.contexts
   *     Optional list of top-level browsing context ids
   *     to subscribe the events for.
   *
   * @throws {InvalidArgumentError}
   *     If <var>events</var> or <var>contexts</var> are not valid types.
   */
  async subscribe(params = {}) {
    const { events, contexts: contextIds = null } = params;

    // Check input types until we run schema validation.
    lazy.assert.array(events, "events: array value expected");
    events.forEach(name => {
      lazy.assert.string(name, `${name}: string value expected`);
    });

    if (contextIds !== null) {
      lazy.assert.array(contextIds, "contexts: array value expected");
      contextIds.forEach(contextId => {
        lazy.assert.string(contextId, `${contextId}: string value expected`);
      });
    }

    const listeners = this.#updateEventMap(events, contextIds, true);

    // TODO: Bug 1801284. Add subscribe priority sorting of subscribeStepEvents (step 4 to 6, and 8).

    // Subscribe to the relevant engine-internal events.
    await this.messageHandler.eventsDispatcher.update(listeners);
  }

  /**
   * Disable certain events either globally, or for a list of browsing contexts.
   *
   * @param {object=} params
   * @param {Array<string>} params.events
   *     List of events to unsubscribe from.
   * @param {Array<string>=} params.contexts
   *     Optional list of top-level browsing context ids
   *     to unsubscribe the events from.
   *
   * @throws {InvalidArgumentError}
   *     If <var>events</var> or <var>contexts</var> are not valid types.
   */
  async unsubscribe(params = {}) {
    const { events, contexts: contextIds = null } = params;

    // Check input types until we run schema validation.
    lazy.assert.array(events, "events: array value expected");
    events.forEach(name => {
      lazy.assert.string(name, `${name}: string value expected`);
    });
    if (contextIds !== null) {
      lazy.assert.array(contextIds, "contexts: array value expected");
      contextIds.forEach(contextId => {
        lazy.assert.string(contextId, `${contextId}: string value expected`);
      });
    }

    const listeners = this.#updateEventMap(events, contextIds, false);

    // Unsubscribe from the relevant engine-internal events.
    await this.messageHandler.eventsDispatcher.update(listeners);
  }

  #assertModuleSupportsEvent(moduleName, event) {
    const rootModuleClass = this.#getRootModuleClass(moduleName);
    if (!rootModuleClass?.supportsEvent(event)) {
      throw new lazy.error.InvalidArgumentError(
        `${event} is not a valid event name`
      );
    }
  }

  #getBrowserIdForContextId(contextId) {
    const context = lazy.TabManager.getBrowsingContextById(contextId);
    if (!context) {
      throw new lazy.error.NoSuchFrameError(
        `Browsing context with id ${contextId} not found`
      );
    }

    return context.browserId;
  }

  #getRootModuleClass(moduleName) {
    // Modules which support event subscriptions should have a root module
    // defining supported events.
    const rootDestination = { type: lazy.RootMessageHandler.type };
    const moduleClasses = this.messageHandler.getAllModuleClasses(
      moduleName,
      rootDestination
    );

    if (!moduleClasses.length) {
      throw new lazy.error.InvalidArgumentError(
        `Module ${moduleName} does not exist`
      );
    }

    return moduleClasses[0];
  }

  #getTopBrowsingContextId(contextId) {
    const context = lazy.TabManager.getBrowsingContextById(contextId);
    if (!context) {
      throw new lazy.error.NoSuchFrameError(
        `Browsing context with id ${contextId} not found`
      );
    }
    const topContext = context.top;
    return lazy.TabManager.getIdForBrowsingContext(topContext);
  }

  /**
   * Obtain a set of events based on the given event name.
   *
   * Could contain a period for a specific event,
   * or just the module name for all events.
   *
   * @param {string} event
   *     Name of the event to process.
   *
   * @returns {Set<string>}
   *     A Set with the expanded events in the form of `<module>.<event>`.
   *
   * @throws {InvalidArgumentError}
   *     If <var>event</var> does not reference a valid event.
   */
  #obtainEvents(event) {
    const events = new Set();

    // Check if a period is present that splits the event name into the module,
    // and the actual event. Hereby only care about the first found instance.
    const index = event.indexOf(".");
    if (index >= 0) {
      const [moduleName] = event.split(".");
      this.#assertModuleSupportsEvent(moduleName, event);
      events.add(event);
    } else {
      // Interpret the name as module, and register all its available events
      const rootModuleClass = this.#getRootModuleClass(event);
      const supportedEvents = rootModuleClass?.supportedEvents;

      for (const eventName of supportedEvents) {
        events.add(eventName);
      }
    }

    return events;
  }

  /**
   * Obtain a list of event enabled browsing context ids.
   *
   * @see https://w3c.github.io/webdriver-bidi/#event-enabled-browsing-contexts
   *
   * @param {string} eventName
   *     The name of the event.
   *
   * @returns {Set<string>} The set of browsing context.
   */
  #obtainEventEnabledBrowsingContextIds(eventName) {
    const contextIds = new Set();
    for (const [
      contextId,
      events,
    ] of this.#browsingContextIdEventMap.entries()) {
      if (events.has(eventName)) {
        // Check that a browsing context still exists for a given id
        const context = lazy.TabManager.getBrowsingContextById(contextId);
        if (context) {
          contextIds.add(contextId);
        }
      }
    }

    return contextIds;
  }

  #onMessageHandlerEvent = (name, event) => {
    this.messageHandler.emitProtocolEvent(name, event);
  };

  /**
   * Update global event state for top-level browsing contexts.
   *
   * @see https://w3c.github.io/webdriver-bidi/#update-the-event-map
   *
   * @param {Array<string>} requestedEventNames
   *     The list of the event names to run the update for.
   * @param {Array<string>|null} browsingContextIds
   *     The list of the browsing context ids to update or null.
   * @param {boolean} enabled
   *     True, if events have to be enabled. Otherwise false.
   *
   * @returns {Array<Subscription>} subscriptions
   *     The list of information to subscribe/unsubscribe to.
   *
   * @throws {InvalidArgumentError}
   *     If failed unsubscribe from event from <var>requestedEventNames</var> for
   *     browsing context id from <var>browsingContextIds</var>, if present.
   */
  #updateEventMap(requestedEventNames, browsingContextIds, enabled) {
    const globalEventSet = new Set(this.#globalEventSet);
    const eventMap = structuredClone(this.#browsingContextIdEventMap);

    const eventNames = new Set();

    requestedEventNames.forEach(name => {
      this.#obtainEvents(name).forEach(event => eventNames.add(event));
    });
    const enabledEvents = new Map();
    const subscriptions = [];

    if (browsingContextIds === null) {
      // Subscribe or unsubscribe events for all browsing contexts.
      if (enabled) {
        // Subscribe to each event.

        // Get the list of all top level browsing context ids.
        const allTopBrowsingContextIds = lazy.TabManager.allBrowserUniqueIds;

        for (const eventName of eventNames) {
          if (!globalEventSet.has(eventName)) {
            const alreadyEnabledContextIds =
              this.#obtainEventEnabledBrowsingContextIds(eventName);
            globalEventSet.add(eventName);
            for (const contextId of alreadyEnabledContextIds) {
              eventMap.get(contextId).delete(eventName);

              // Since we're going to subscribe to all top-level
              // browsing context ids to not have duplicate subscriptions,
              // we have to unsubscribe from already subscribed.
              subscriptions.push({
                event: eventName,
                contextDescriptor: {
                  type: lazy.ContextDescriptorType.TopBrowsingContext,
                  id: this.#getBrowserIdForContextId(contextId),
                },
                callback: this.#onMessageHandlerEvent,
                enable: false,
              });
            }

            // Get a list of all top-level browsing context ids
            // that are not contained in alreadyEnabledContextIds.
            const newlyEnabledContextIds = allTopBrowsingContextIds.filter(
              contextId => !alreadyEnabledContextIds.has(contextId)
            );

            enabledEvents.set(eventName, newlyEnabledContextIds);

            subscriptions.push({
              event: eventName,
              contextDescriptor: {
                type: lazy.ContextDescriptorType.All,
              },
              callback: this.#onMessageHandlerEvent,
              enable: true,
            });
          }
        }
      } else {
        // Unsubscribe each event which has a global subscription.
        for (const eventName of eventNames) {
          if (globalEventSet.has(eventName)) {
            globalEventSet.delete(eventName);

            subscriptions.push({
              event: eventName,
              contextDescriptor: {
                type: lazy.ContextDescriptorType.All,
              },
              callback: this.#onMessageHandlerEvent,
              enable: false,
            });
          } else {
            throw new lazy.error.InvalidArgumentError(
              `Failed to unsubscribe from event ${eventName}`
            );
          }
        }
      }
    } else {
      // Subscribe or unsubscribe events for given list of browsing context ids.
      const targets = new Map();
      for (const contextId of browsingContextIds) {
        const topLevelContextId = this.#getTopBrowsingContextId(contextId);
        if (!eventMap.has(topLevelContextId)) {
          eventMap.set(topLevelContextId, new Set());
        }
        targets.set(topLevelContextId, eventMap.get(topLevelContextId));
      }

      for (const eventName of eventNames) {
        // Do nothing if we want to subscribe,
        // but the event has already a global subscription.
        if (enabled && this.#globalEventSet.has(eventName)) {
          continue;
        }
        for (const [contextId, target] of targets.entries()) {
          // Subscribe if an event doesn't have a subscription for a specific context id.
          if (enabled && !target.has(eventName)) {
            target.add(eventName);
            if (!enabledEvents.has(eventName)) {
              enabledEvents.set(eventName, new Set());
            }
            enabledEvents.get(eventName).add(contextId);

            subscriptions.push({
              event: eventName,
              contextDescriptor: {
                type: lazy.ContextDescriptorType.TopBrowsingContext,
                id: this.#getBrowserIdForContextId(contextId),
              },
              callback: this.#onMessageHandlerEvent,
              enable: true,
            });
          } else if (!enabled) {
            // Unsubscribe from each event for a specific context id if the event has a subscription.
            if (target.has(eventName)) {
              target.delete(eventName);

              subscriptions.push({
                event: eventName,
                contextDescriptor: {
                  type: lazy.ContextDescriptorType.TopBrowsingContext,
                  id: this.#getBrowserIdForContextId(contextId),
                },
                callback: this.#onMessageHandlerEvent,
                enable: false,
              });
            } else {
              throw new lazy.error.InvalidArgumentError(
                `Failed to unsubscribe from event ${eventName} for context ${contextId}`
              );
            }
          }
        }
      }
    }

    this.#globalEventSet = globalEventSet;
    this.#browsingContextIdEventMap = eventMap;

    return subscriptions;
  }
}

// To export the class as lower-case
export const session = SessionModule;
