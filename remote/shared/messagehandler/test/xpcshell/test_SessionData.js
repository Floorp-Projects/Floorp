/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ContextDescriptorType } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs"
);
const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);
const { SessionData } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/sessiondata/SessionData.sys.mjs"
);

add_task(async function test_sessionData() {
  const sessionData = new SessionData(new RootMessageHandler("session-id-1"));
  equal(sessionData.getSessionData("mod", "event").length, 0);

  const globalContext = {
    type: ContextDescriptorType.All,
  };
  const otherContext = { type: "other-type", id: "some-id" };

  info("Add a first event for the global context");
  let addedValues = sessionData.addSessionData("mod", "event", globalContext, [
    "first.event",
  ]);
  equal(addedValues.length, 1, "One value added");
  equal(addedValues[0], "first.event", "Expected value was added");
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: globalContext,
    },
  ]);

  info("Add the exact same data (same module, type, context, value)");
  addedValues = sessionData.addSessionData("mod", "event", globalContext, [
    "first.event",
  ]);
  equal(addedValues.length, 0, "No new value added");
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: globalContext,
    },
  ]);

  info("Add another context for the same event");
  addedValues = sessionData.addSessionData("mod", "event", otherContext, [
    "first.event",
  ]);
  equal(addedValues.length, 1, "One value added");
  equal(addedValues[0], "first.event", "Expected value was added");
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
  addedValues = sessionData.addSessionData("mod", "event", globalContext, [
    "second.event",
  ]);
  equal(addedValues.length, 1, "One value added");
  equal(addedValues[0], "second.event", "Expected value was added");
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
  addedValues = sessionData.addSessionData("mod", "event", globalContext, [
    "third.event",
    "fourth.event",
  ]);
  equal(addedValues.length, 2, "Two values added");
  equal(addedValues[0], "third.event", "Expected value was added");
  equal(addedValues[1], "fourth.event", "Expected value was added");
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
  let removedValues = sessionData.removeSessionData(
    "mod",
    "event",
    globalContext,
    ["second.event", "third.event", "fourth.event"]
  );
  equal(removedValues.length, 3, "Three values removed");
  equal(removedValues[0], "second.event", "Expected value was removed");
  equal(removedValues[1], "third.event", "Expected value was removed");
  equal(removedValues[2], "fourth.event", "Expected value was removed");
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
  removedValues = sessionData.removeSessionData("mod", "event", globalContext, [
    "first.event",
  ]);
  equal(removedValues.length, 1, "One value removed");
  equal(removedValues[0], "first.event", "Expected value was removed");
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: otherContext,
    },
  ]);

  info("Remove the other context from the first event");
  sessionData.removeSessionData("mod", "event", otherContext, ["first.event"]);
  equal(removedValues.length, 1, "One value removed");
  equal(removedValues[0], "first.event", "Expected value was removed");
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
