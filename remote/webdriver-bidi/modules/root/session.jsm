/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["session"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm",
});

class SessionModule extends Module {
  destroy() {}

  /**
   * Commands
   */

  /**
   * Enable certain events either globally, or for a list of browsing contexts.
   *
   * @params {Object=} params
   * @params {Array<String>} events
   *     List of events to subscribe to.
   * @params {Array<String>=} contexts
   *     Optional list of top-level browsing context ids
   *     to subscribe the events for.
   *
   * @throws {InvalidArgumentError}
   *     If <var>events</var> or <var>contexts</var> are not valid types.
   */
  async subscribe(params = {}) {
    const { events, contexts = [] } = params;

    // Check input types until we run schema validation.
    lazy.assert.array(events, "events: array value expected");
    events.forEach(name => {
      lazy.assert.string(name, `${name}: string value expected`);
    });

    lazy.assert.array(contexts, "contexts: array value expected");
    contexts.forEach(context => {
      lazy.assert.string(context, `${context}: string value expected`);
    });

    // For now just subscribe the events to all available top-level
    // browsing contexts.
    const allEvents = events
      .map(event => Array.from(this.#obtainEvents(event)))
      .flat();
    await Promise.allSettled(
      allEvents.map(event => {
        const [moduleName] = event.split(".");
        this.#assertModuleSupportsEvent(moduleName, event);

        return this.messageHandler.handleCommand({
          moduleName,
          commandName: "_subscribeEvent",
          params: {
            event,
          },
          destination: {
            type: lazy.RootMessageHandler.type,
          },
        });
      })
    );
  }

  /**
   * Disable certain events either globally, or for a list of browsing contexts.
   *
   * @params {Object=} params
   * @params {Array<String>} events
   *     List of events to unsubscribe from.
   * @params {Array<String>=} contexts
   *     Optional list of top-level browsing context ids
   *     to unsubscribe the events from.
   *
   * @throws {InvalidArgumentError}
   *     If <var>events</var> or <var>contexts</var> are not valid types.
   */
  async unsubscribe(params = {}) {
    const { events, contexts = [] } = params;

    // Check input types until we run schema validation.
    lazy.assert.array(events, "events: array value expected");
    events.forEach(name => {
      lazy.assert.string(name, `${name}: string value expected`);
    });

    lazy.assert.array(contexts, "contexts: array value expected");
    contexts.forEach(context => {
      lazy.assert.string(context, `${context}: string value expected`);
    });

    // For now just unsubscribe the events from all available top-level
    // browsing contexts.
    const allEvents = events
      .map(event => Array.from(this.#obtainEvents(event)))
      .flat();
    await Promise.allSettled(
      allEvents.map(event => {
        const [moduleName] = event.split(".");
        this.#assertModuleSupportsEvent(moduleName, event);

        return this.messageHandler.handleCommand({
          moduleName,
          commandName: "_unsubscribeEvent",
          params: {
            event,
          },
          destination: {
            type: lazy.RootMessageHandler.type,
          },
        });
      })
    );
  }

  #assertModuleSupportsEventSubscription(moduleName) {
    const rootModuleClass = this.#getRootModuleClass(moduleName);
    const supportsEvents = rootModuleClass?.supportsCommand("_subscribeEvent");
    if (!supportsEvents) {
      throw new lazy.error.InvalidArgumentError(
        `Module ${moduleName} does not support event subscriptions`
      );
    }
  }

  #assertModuleSupportsEvent(moduleName, event) {
    const rootModuleClass = this.#getRootModuleClass(moduleName);
    const supportsEvent = rootModuleClass?.supportsEvent(event);
    if (!supportsEvent) {
      throw new lazy.error.InvalidArgumentError(
        `Module ${moduleName} does not support event ${event}`
      );
    }
  }

  #getRootModuleClass(moduleName) {
    // Modules which support event subscriptions should have a root module
    // defining supported events.
    const rootDestination = { type: lazy.RootMessageHandler.type };
    const moduleClasses = this.messageHandler.getAllModuleClasses(
      moduleName,
      rootDestination
    );

    return moduleClasses[0];
  }

  /**
   * Obtain a set of events based on the given event name.
   *
   * Could contain a period for a
   *     specific event, or just the module name for all events.
   *
   * @param {String} event
   *     Name of the event to process.
   *
   * @returns {Set<String>}
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
      // TODO: Throw invalid argument error if event doesn't exist
      events.add(event);
    } else {
      // Interpret the name as module, and register all its available events
      this.#assertModuleSupportsEventSubscription(event);
      // TODO: Append all available events from the module
    }

    return events;
  }
}

// To export the class as lower-case
const session = SessionModule;
