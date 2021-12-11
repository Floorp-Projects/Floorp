/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["session"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  Module: "chrome://remote/content/shared/messagehandler/Module.jsm",
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm",
});

class Session extends Module {
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
    assert.array(events, "events: array value expected");
    events.forEach(name => {
      assert.string(name, `${name}: string value expected`);
    });

    assert.array(contexts, "contexts: array value expected");
    contexts.forEach(context => {
      assert.string(context, `${context}: string value expected`);
    });

    // For now just subscribe the events to all available top-level
    // browsing contexts.
    const allEvents = events
      .map(event => Array.from(obtainEvents(event)))
      .flat();
    await Promise.allSettled(
      allEvents.map(event => {
        const [moduleName] = event.split(".");
        return this.messageHandler.handleCommand({
          moduleName,
          commandName: "_subscribeEvent",
          params: {
            event,
          },
          destination: {
            type: RootMessageHandler.type,
          },
        });
      })
    );
  }
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
function obtainEvents(event) {
  const events = new Set();

  // Check if a period is present that splits the event name into the module,
  // and the actual event. Hereby only care about the first found instance.
  const index = event.indexOf(".");
  if (index >= 0) {
    // TODO: Throw invalid argument error if event doesn't exist
    events.add(event);
  } else {
    // Interpret the name as module, and register all its available events
    // TODO: Throw invalid argument error if not a valid module name
    // TODO: Append all available events from the module
  }

  return events;
}

// To export the class as lower-case
const session = Session;
