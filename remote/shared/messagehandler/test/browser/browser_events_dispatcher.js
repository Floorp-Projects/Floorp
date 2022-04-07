/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { WindowGlobalMessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.jsm"
);

/**
 * Check the basic behavior of on/off.
 */
add_task(async function test_add_remove_internal_event_listener() {
  const tab = await addTab("https://example.com/document-builder.sjs?html=tab");
  const browsingContext = tab.linkedBrowser.browsingContext;
  const contextDescriptor = {
    type: ContextDescriptorType.TopBrowsingContext,
    id: browsingContext.browserId,
  };

  const root = createRootMessageHandler("session-id-event");
  const monitoringEvents = await setupEventMonitoring(root);
  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(await isSubscribed(root, browsingContext), false);

  info("Add an listener for internaleventemitter.testEvent");
  const internalEvents = [];
  const onInternalEvent = (event, data) => internalEvents.push(data.text);
  await root.eventsDispatcher.on(
    "internaleventemitter.testEvent",
    contextDescriptor,
    onInternalEvent
  );
  is(await isSubscribed(root, browsingContext), true);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(internalEvents.length, 1);

  info(
    "Remove a listener for a callback not added before and check that the first one is still registered"
  );
  const anotherCallback = () => {};
  await root.eventsDispatcher.off(
    "internaleventemitter.testEvent",
    contextDescriptor,
    anotherCallback
  );
  is(await isSubscribed(root, browsingContext), true);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(internalEvents.length, 2);

  info("Remove the listener for internaleventemitter.testEvent");
  await root.eventsDispatcher.off(
    "internaleventemitter.testEvent",
    contextDescriptor,
    onInternalEvent
  );
  is(await isSubscribed(root, browsingContext), false);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(internalEvents.length, 2);

  info("Add the listener for internaleventemitter.testEvent again");
  await root.eventsDispatcher.on(
    "internaleventemitter.testEvent",
    contextDescriptor,
    onInternalEvent
  );
  is(await isSubscribed(root, browsingContext), true);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(internalEvents.length, 3);

  info("Remove the listener for internaleventemitter.testEvent");
  await root.eventsDispatcher.off(
    "internaleventemitter.testEvent",
    contextDescriptor,
    onInternalEvent
  );
  is(await isSubscribed(root, browsingContext), false);

  info("Remove the listener again to check the API will not throw");
  await root.eventsDispatcher.off(
    "internaleventemitter.testEvent",
    contextDescriptor,
    onInternalEvent
  );

  root.destroy();
  gBrowser.removeTab(tab);
});

/**
 * Check that two callbacks can subscribe to the same event in the same context
 * in parallel.
 */
add_task(async function test_two_callbacks() {
  const tab = await addTab("https://example.com/document-builder.sjs?html=tab");
  const browsingContext = tab.linkedBrowser.browsingContext;
  const contextDescriptor = {
    type: ContextDescriptorType.TopBrowsingContext,
    id: browsingContext.browserId,
  };

  const root = createRootMessageHandler("session-id-event");
  const monitoringEvents = await setupEventMonitoring(root);

  info("Add an listener for internaleventemitter.testEvent");
  const internalEvents = [];
  const onInternalEvent = (event, data) => internalEvents.push(data.text);
  await root.eventsDispatcher.on(
    "internaleventemitter.testEvent",
    contextDescriptor,
    onInternalEvent
  );

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(internalEvents.length, 1);

  info("Add another listener for internaleventemitter.testEvent");
  const otherInternalEvents = [];
  const otherCallback = (event, data) => otherInternalEvents.push(data.text);
  await root.eventsDispatcher.on(
    "internaleventemitter.testEvent",
    contextDescriptor,
    otherCallback
  );
  is(await isSubscribed(root, browsingContext), true);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(internalEvents.length, 2);
  is(otherInternalEvents.length, 1);

  info("Remove the other listener for internaleventemitter.testEvent");
  await root.eventsDispatcher.off(
    "internaleventemitter.testEvent",
    contextDescriptor,
    otherCallback
  );
  is(await isSubscribed(root, browsingContext), true);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(internalEvents.length, 3);
  is(otherInternalEvents.length, 1);

  info("Remove the first listener for internaleventemitter.testEvent");
  await root.eventsDispatcher.off(
    "internaleventemitter.testEvent",
    contextDescriptor,
    onInternalEvent
  );
  is(await isSubscribed(root, browsingContext), false);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(internalEvents.length, 3);
  is(otherInternalEvents.length, 1);

  root.destroy();
  gBrowser.removeTab(tab);
});

/**
 * Check that two callbacks can subscribe to the same event in the two contexts.
 */
add_task(async function test_two_contexts() {
  const tab1 = await addTab("https://example.com/document-builder.sjs?html=1");
  const browsingContext1 = tab1.linkedBrowser.browsingContext;

  const tab2 = await addTab("https://example.com/document-builder.sjs?html=2");
  const browsingContext2 = tab2.linkedBrowser.browsingContext;

  const contextDescriptor1 = {
    type: ContextDescriptorType.TopBrowsingContext,
    id: browsingContext1.browserId,
  };
  const contextDescriptor2 = {
    type: ContextDescriptorType.TopBrowsingContext,
    id: browsingContext2.browserId,
  };

  const root = createRootMessageHandler("session-id-event");

  const monitoringEvents = await setupEventMonitoring(root);

  const internalEvents1 = [];
  const onInternalEvent1 = (event, data) => internalEvents1.push(data.text);
  await root.eventsDispatcher.on(
    "internaleventemitter.testEvent",
    contextDescriptor1,
    onInternalEvent1
  );
  is(await isSubscribed(root, browsingContext1), true);
  is(await isSubscribed(root, browsingContext2), false);

  const internalEvents2 = [];
  const onInternalEvent2 = (event, data) => internalEvents2.push(data.text);
  await root.eventsDispatcher.on(
    "internaleventemitter.testEvent",
    contextDescriptor2,
    onInternalEvent2
  );
  is(await isSubscribed(root, browsingContext1), true);
  is(await isSubscribed(root, browsingContext2), true);

  // Note that events are not filtered by context at the moment, even though
  // a context descriptor is provided to on/off.
  // Consumers are still responsible for checking that the internal event
  // matches the correct context.
  // Consequently, emitting an event on browsingContext1 will trigger both
  // callbacks.
  // TODO: This should be handled by the framework in Bug 1763137.
  await emitTestEvent(root, browsingContext1, monitoringEvents);
  is(internalEvents1.length, 1);
  is(internalEvents2.length, 1);
  await emitTestEvent(root, browsingContext2, monitoringEvents);
  is(internalEvents1.length, 2);
  is(internalEvents2.length, 2);

  await root.eventsDispatcher.off(
    "internaleventemitter.testEvent",
    contextDescriptor1,
    onInternalEvent1
  );
  is(await isSubscribed(root, browsingContext1), false);
  is(await isSubscribed(root, browsingContext2), true);

  // No event expected here since the module for browsingContext1 is no longer
  // subscribed
  await emitTestEvent(root, browsingContext1, monitoringEvents);
  is(internalEvents1.length, 2);
  is(internalEvents2.length, 2);

  // Whereas the module for browsingContext2 is still subscribed
  await emitTestEvent(root, browsingContext2, monitoringEvents);
  is(internalEvents1.length, 2);
  is(internalEvents2.length, 3);

  await root.eventsDispatcher.off(
    "internaleventemitter.testEvent",
    contextDescriptor2,
    onInternalEvent2
  );
  is(await isSubscribed(root, browsingContext1), false);
  is(await isSubscribed(root, browsingContext2), false);

  await emitTestEvent(root, browsingContext1, monitoringEvents);
  await emitTestEvent(root, browsingContext2, monitoringEvents);
  is(internalEvents1.length, 2);
  is(internalEvents2.length, 3);

  root.destroy();
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab1);
});

async function setupEventMonitoring(root) {
  const monitoringEvents = [];
  const onMonitoringEvent = (event, data) => monitoringEvents.push(data.text);
  root.on("internaleventemitter.monitoringEvent", onMonitoringEvent);

  registerCleanupFunction(() =>
    root.off("internaleventemitter.monitoringEvent", onMonitoringEvent)
  );

  return monitoringEvents;
}

async function emitTestEvent(root, browsingContext, monitoringEvents) {
  const count = monitoringEvents.length;
  info("Call internaleventemitter.emitTestEvent");
  await root.handleCommand({
    moduleName: "internaleventemitter",
    commandName: "emitTestEvent",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
  });

  // The monitoring event is always emitted, regardless of the status of the
  // module. Wait for catching this event before resuming the assertions.
  info("Wait for the monitoring event");
  await BrowserTestUtils.waitForCondition(
    () => monitoringEvents.length >= count + 1
  );
  is(monitoringEvents.length, count + 1);
}

function isSubscribed(root, browsingContext) {
  info("Call internaleventemitter.isSubscribed");
  return root.handleCommand({
    moduleName: "internaleventemitter",
    commandName: "isSubscribed",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
  });
}
