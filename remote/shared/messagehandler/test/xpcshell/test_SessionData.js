/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { CONTEXT_DESCRIPTOR_TYPES } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/MessageHandler.jsm"
);
const { RootMessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm"
);
const { SessionData } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/sessiondata/SessionData.jsm"
);

add_task(async function test_sessionData() {
  const sessionData = new SessionData(new RootMessageHandler("session-id-1"));
  equal(sessionData.getSessionData("mod", "event").length, 0);

  const globalContext = {
    type: CONTEXT_DESCRIPTOR_TYPES.ALL,
  };
  const otherContext = { type: "other-type", id: "some-id" };

  info("Add a first event for the global context");
  sessionData.addSessionData("mod", "event", globalContext, ["first.event"]);
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: globalContext,
    },
  ]);

  info("Add the exact same data (same module, type, context, value)");
  sessionData.addSessionData("mod", "event", globalContext, ["first.event"]);
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: globalContext,
    },
  ]);

  info("Add another context for the same event");
  sessionData.addSessionData("mod", "event", otherContext, ["first.event"]);
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: globalContext,
    },
    {
      value: "first.event",
      contextDescriptor: otherContext,
    },
  ]);

  info("Add a second event for the global context");
  sessionData.addSessionData("mod", "event", globalContext, ["second.event"]);
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: globalContext,
    },
    {
      value: "first.event",
      contextDescriptor: otherContext,
    },
    {
      value: "second.event",
      contextDescriptor: globalContext,
    },
  ]);

  info("Add two events for the global context");
  sessionData.addSessionData("mod", "event", globalContext, [
    "third.event",
    "fourth.event",
  ]);
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: globalContext,
    },
    {
      value: "first.event",
      contextDescriptor: otherContext,
    },
    {
      value: "second.event",
      contextDescriptor: globalContext,
    },
    {
      value: "third.event",
      contextDescriptor: globalContext,
    },
    {
      value: "fourth.event",
      contextDescriptor: globalContext,
    },
  ]);

  info("Remove the second, third and fourth events");
  sessionData.removeSessionData("mod", "event", globalContext, [
    "second.event",
    "third.event",
    "fourth.event",
  ]);
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: globalContext,
    },
    {
      value: "first.event",
      contextDescriptor: otherContext,
    },
  ]);

  info("Remove the global context from the first event");
  sessionData.removeSessionData("mod", "event", globalContext, ["first.event"]);
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: otherContext,
    },
  ]);

  info("Remove the other context from the first event");
  sessionData.removeSessionData("mod", "event", otherContext, ["first.event"]);
  checkEvents(sessionData.getSessionData("mod", "event"), []);
});

function checkEvents(events, expectedEvents) {
  // Check the arrays have the same size.
  equal(events.length, expectedEvents.length);

  // Check all the expectedEvents can be found in the events array.
  for (const expected of expectedEvents) {
    ok(
      events.some(
        event =>
          expected.contextDescriptor.type === event.contextDescriptor.type &&
          expected.contextDescriptor.id === event.contextDescriptor.id &&
          expected.value == event.value
      )
    );
  }
}
