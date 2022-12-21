/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ContextDescriptorType } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs"
);
const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);
const { SessionData, SessionDataMethod } = ChromeUtils.importESModule(
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
  let updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Add, globalContext, ["first.event"]),
  ]);
  equal(updatedItems.length, 1, "One item updated");
  equal(updatedItems[0].method, SessionDataMethod.Add, "One item added");
  let updatedValues = updatedItems[0].values;
  equal(updatedValues.length, 1, "One value added");
  equal(updatedValues[0], "first.event", "Expected value was added");
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: globalContext,
    },
  ]);

  info("Add the exact same data (same module, type, context, value)");
  updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Add, globalContext, ["first.event"]),
  ]);
  equal(updatedItems.length, 0, "No new item updated");
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: globalContext,
    },
  ]);

  info("Add another context for the same event");
  updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Add, otherContext, ["first.event"]),
  ]);
  equal(updatedItems.length, 1, "One item updated");
  equal(updatedItems[0].method, SessionDataMethod.Add, "One item added");
  updatedValues = updatedItems[0].values;
  equal(updatedValues.length, 1, "One value added");
  equal(updatedValues[0], "first.event", "Expected value was added");
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
  updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Add, globalContext, ["second.event"]),
  ]);
  equal(updatedItems.length, 1, "One item updated");
  equal(updatedItems[0].method, SessionDataMethod.Add, "One item added");
  updatedValues = updatedItems[0].values;
  equal(updatedValues.length, 1, "One value added");
  equal(updatedValues[0], "second.event", "Expected value was added");
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
  updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Add, globalContext, [
      "third.event",
      "fourth.event",
    ]),
  ]);
  equal(updatedItems.length, 1, "One item updated");
  equal(updatedItems[0].method, SessionDataMethod.Add, "One item added");
  updatedValues = updatedItems[0].values;
  equal(updatedValues.length, 2, "Two values added");
  equal(updatedValues[0], "third.event", "Expected value was added");
  equal(updatedValues[1], "fourth.event", "Expected value was added");
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
  updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Remove, globalContext, [
      "second.event",
      "third.event",
      "fourth.event",
    ]),
  ]);
  equal(updatedItems.length, 1, "One item updated");
  equal(updatedItems[0].method, SessionDataMethod.Remove, "One item removed");
  updatedValues = updatedItems[0].values;
  equal(updatedValues.length, 3, "Three values removed");
  equal(updatedValues[0], "second.event", "Expected value was removed");
  equal(updatedValues[1], "third.event", "Expected value was removed");
  equal(updatedValues[2], "fourth.event", "Expected value was removed");
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
  updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Remove, globalContext, ["first.event"]),
  ]);
  equal(updatedItems.length, 1, "One item updated");
  equal(updatedItems[0].method, SessionDataMethod.Remove, "One item removed");
  updatedValues = updatedItems[0].values;
  equal(updatedValues.length, 1, "One value removed");
  equal(updatedValues[0], "first.event", "Expected value was removed");
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: otherContext,
    },
  ]);

  info("Remove the other context from the first event");
  updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Remove, otherContext, ["first.event"]),
  ]);
  equal(updatedItems.length, 1, "One item updated");
  equal(updatedItems[0].method, SessionDataMethod.Remove, "One item removed");
  updatedValues = updatedItems[0].values;
  equal(updatedValues.length, 1, "One value removed");
  equal(updatedValues[0], "first.event", "Expected value was removed");
  checkEvents(sessionData.getSessionData("mod", "event"), []);

  info("Add two events for different contexts");
  updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Add, otherContext, ["first.event"]),
    createUpdate(SessionDataMethod.Add, globalContext, ["second.event"]),
  ]);
  equal(updatedItems.length, 2, "Two items updated");
  equal(updatedItems[0].method, SessionDataMethod.Add, "First item added");
  updatedValues = updatedItems[0].values;
  equal(updatedValues.length, 1, "One value for first item added");
  equal(updatedValues[0], "first.event", "Expected value first item was added");
  equal(updatedItems[1].method, SessionDataMethod.Add, "Second item added");
  updatedValues = updatedItems[1].values;
  equal(updatedValues.length, 1, "One value for second item added");
  equal(
    updatedValues[0],
    "second.event",
    "Expected value second item was added"
  );
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: otherContext,
    },
    {
      value: "second.event",
      contextDescriptor: globalContext,
    },
  ]);

  info("Remove two events for different contexts");
  updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Remove, otherContext, ["first.event"]),
    createUpdate(SessionDataMethod.Remove, globalContext, ["second.event"]),
  ]);
  equal(updatedItems.length, 2, "Two items updated");
  equal(updatedItems[0].method, SessionDataMethod.Remove, "First item removed");
  updatedValues = updatedItems[0].values;
  equal(updatedValues.length, 1, "One value for first item removed");
  equal(
    updatedValues[0],
    "first.event",
    "Expected value first item was removed"
  );
  equal(
    updatedItems[1].method,
    SessionDataMethod.Remove,
    "Second item removed"
  );
  updatedValues = updatedItems[1].values;
  equal(updatedValues.length, 1, "One value for second item removed");
  equal(
    updatedValues[0],
    "second.event",
    "Expected value second item was removed"
  );
  checkEvents(sessionData.getSessionData("mod", "event"), []);

  info("Add and remove event in different order");
  updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Remove, otherContext, ["first.event"]),
    createUpdate(SessionDataMethod.Add, otherContext, ["first.event"]),
  ]);
  equal(updatedItems.length, 1, "One item updated");
  equal(updatedItems[0].method, SessionDataMethod.Add, "One item added");
  updatedValues = updatedItems[0].values;
  equal(updatedValues.length, 1, "One value added");
  equal(updatedValues[0], "first.event", "Expected value was added");
  checkEvents(sessionData.getSessionData("mod", "event"), [
    {
      value: "first.event",
      contextDescriptor: otherContext,
    },
  ]);

  updatedItems = sessionData.applySessionData([
    createUpdate(SessionDataMethod.Add, otherContext, ["first.event"]),
    createUpdate(SessionDataMethod.Remove, otherContext, ["first.event"]),
  ]);
  equal(updatedItems.length, 1, "No item update");
  equal(updatedItems[0].method, SessionDataMethod.Remove, "One item removed");
  updatedValues = updatedItems[0].values;
  equal(updatedValues.length, 1, "One value removed");
  equal(updatedValues[0], "first.event", "Expected value was removed");
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

function createUpdate(method, contextDescriptor, values) {
  return {
    method,
    moduleName: "mod",
    category: "event",
    contextDescriptor,
    values,
  };
}
