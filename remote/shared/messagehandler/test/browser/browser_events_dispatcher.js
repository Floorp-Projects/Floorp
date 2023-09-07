/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check the basic behavior of on/off.
 */
add_task(async function test_add_remove_event_listener() {
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

  info("Add an listener for eventemitter.testEvent");
  const events = [];
  const onEvent = (event, data) => events.push(data.text);
  await root.eventsDispatcher.on(
    "eventemitter.testEvent",
    contextDescriptor,
    onEvent
  );
  is(await isSubscribed(root, browsingContext), true);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(events.length, 1);

  info(
    "Remove a listener for a callback not added before and check that the first one is still registered"
  );
  const anotherCallback = () => {};
  await root.eventsDispatcher.off(
    "eventemitter.testEvent",
    contextDescriptor,
    anotherCallback
  );
  is(await isSubscribed(root, browsingContext), true);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(events.length, 2);

  info("Remove the listener for eventemitter.testEvent");
  await root.eventsDispatcher.off(
    "eventemitter.testEvent",
    contextDescriptor,
    onEvent
  );
  is(await isSubscribed(root, browsingContext), false);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(events.length, 2);

  info("Add the listener for eventemitter.testEvent again");
  await root.eventsDispatcher.on(
    "eventemitter.testEvent",
    contextDescriptor,
    onEvent
  );
  is(await isSubscribed(root, browsingContext), true);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(events.length, 3);

  info("Remove the listener for eventemitter.testEvent");
  await root.eventsDispatcher.off(
    "eventemitter.testEvent",
    contextDescriptor,
    onEvent
  );
  is(await isSubscribed(root, browsingContext), false);

  info("Remove the listener again to check the API will not throw");
  await root.eventsDispatcher.off(
    "eventemitter.testEvent",
    contextDescriptor,
    onEvent
  );

  root.destroy();
  gBrowser.removeTab(tab);
});

add_task(async function test_has_listener() {
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

  // Shortcut for the EventsDispatcher.hasListener API.
  function hasListener(contextId) {
    return root.eventsDispatcher.hasListener("eventemitter.testEvent", {
      contextId,
    });
  }

  const onEvent = () => {};
  await root.eventsDispatcher.on(
    "eventemitter.testEvent",
    contextDescriptor1,
    onEvent
  );
  ok(hasListener(browsingContext1.id), "Has a listener for browsingContext1");
  ok(!hasListener(browsingContext2.id), "No listener for browsingContext2");

  await root.eventsDispatcher.on(
    "eventemitter.testEvent",
    contextDescriptor2,
    onEvent
  );
  ok(hasListener(browsingContext1.id), "Still a listener for browsingContext1");
  ok(hasListener(browsingContext2.id), "Has a listener for browsingContext2");

  await root.eventsDispatcher.off(
    "eventemitter.testEvent",
    contextDescriptor1,
    onEvent
  );
  ok(!hasListener(browsingContext1.id), "No listener for browsingContext1");
  ok(hasListener(browsingContext2.id), "Still a listener for browsingContext2");

  await root.eventsDispatcher.off(
    "eventemitter.testEvent",
    contextDescriptor2,
    onEvent
  );
  ok(!hasListener(browsingContext1.id), "No listener for browsingContext1");
  ok(!hasListener(browsingContext2.id), "No listener for browsingContext2");

  await root.eventsDispatcher.on(
    "eventemitter.testEvent",
    {
      type: ContextDescriptorType.All,
    },
    onEvent
  );
  ok(hasListener(browsingContext1.id), "Has a listener for browsingContext1");
  ok(hasListener(browsingContext2.id), "Has a listener for browsingContext2");

  await root.eventsDispatcher.off(
    "eventemitter.testEvent",
    {
      type: ContextDescriptorType.All,
    },
    onEvent
  );
  ok(!hasListener(browsingContext1.id), "No listener for browsingContext1");
  ok(!hasListener(browsingContext2.id), "No listener for browsingContext2");

  root.destroy();
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab1);
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

  info("Add an listener for eventemitter.testEvent");
  const events = [];
  const onEvent = (event, data) => events.push(data.text);
  await root.eventsDispatcher.on(
    "eventemitter.testEvent",
    contextDescriptor,
    onEvent
  );

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(events.length, 1);

  info("Add another listener for eventemitter.testEvent");
  const otherevents = [];
  const otherCallback = (event, data) => otherevents.push(data.text);
  await root.eventsDispatcher.on(
    "eventemitter.testEvent",
    contextDescriptor,
    otherCallback
  );
  is(await isSubscribed(root, browsingContext), true);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(events.length, 2);
  is(otherevents.length, 1);

  info("Remove the other listener for eventemitter.testEvent");
  await root.eventsDispatcher.off(
    "eventemitter.testEvent",
    contextDescriptor,
    otherCallback
  );
  is(await isSubscribed(root, browsingContext), true);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(events.length, 3);
  is(otherevents.length, 1);

  info("Remove the first listener for eventemitter.testEvent");
  await root.eventsDispatcher.off(
    "eventemitter.testEvent",
    contextDescriptor,
    onEvent
  );
  is(await isSubscribed(root, browsingContext), false);

  await emitTestEvent(root, browsingContext, monitoringEvents);
  is(events.length, 3);
  is(otherevents.length, 1);

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

  const events1 = [];
  const onEvent1 = (event, data) => events1.push(data.text);
  await root.eventsDispatcher.on(
    "eventemitter.testEvent",
    contextDescriptor1,
    onEvent1
  );
  is(await isSubscribed(root, browsingContext1), true);
  is(await isSubscribed(root, browsingContext2), false);

  const events2 = [];
  const onEvent2 = (event, data) => events2.push(data.text);
  await root.eventsDispatcher.on(
    "eventemitter.testEvent",
    contextDescriptor2,
    onEvent2
  );
  is(await isSubscribed(root, browsingContext1), true);
  is(await isSubscribed(root, browsingContext2), true);

  await emitTestEvent(root, browsingContext1, monitoringEvents);
  is(events1.length, 1);
  is(events2.length, 0);

  await emitTestEvent(root, browsingContext2, monitoringEvents);
  is(events1.length, 1);
  is(events2.length, 1);

  await root.eventsDispatcher.off(
    "eventemitter.testEvent",
    contextDescriptor1,
    onEvent1
  );
  is(await isSubscribed(root, browsingContext1), false);
  is(await isSubscribed(root, browsingContext2), true);

  // No event expected here since the module for browsingContext1 is no longer
  // subscribed
  await emitTestEvent(root, browsingContext1, monitoringEvents);
  is(events1.length, 1);
  is(events2.length, 1);

  // Whereas the module for browsingContext2 is still subscribed
  await emitTestEvent(root, browsingContext2, monitoringEvents);
  is(events1.length, 1);
  is(events2.length, 2);

  await root.eventsDispatcher.off(
    "eventemitter.testEvent",
    contextDescriptor2,
    onEvent2
  );
  is(await isSubscribed(root, browsingContext1), false);
  is(await isSubscribed(root, browsingContext2), false);

  await emitTestEvent(root, browsingContext1, monitoringEvents);
  await emitTestEvent(root, browsingContext2, monitoringEvents);
  is(events1.length, 1);
  is(events2.length, 2);

  root.destroy();
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab1);
});

/**
 * Check that adding and removing first listener for the specific context and then
 * for the global context works as expected.
 */
add_task(
  async function test_remove_context_event_listener_and_then_global_event_listener() {
    const tab = await addTab(
      "https://example.com/document-builder.sjs?html=tab"
    );
    const browsingContext = tab.linkedBrowser.browsingContext;
    const contextDescriptor = {
      type: ContextDescriptorType.TopBrowsingContext,
      id: browsingContext.browserId,
    };
    const contextDescriptorAll = {
      type: ContextDescriptorType.All,
    };

    const root = createRootMessageHandler("session-id-event");
    const monitoringEvents = await setupEventMonitoring(root);
    await emitTestEvent(root, browsingContext, monitoringEvents);
    is(await isSubscribed(root, browsingContext), false);

    info("Add an listener for eventemitter.testEvent");
    const events = [];
    const onEvent = (event, data) => events.push(data.text);
    await root.eventsDispatcher.on(
      "eventemitter.testEvent",
      contextDescriptor,
      onEvent
    );
    is(await isSubscribed(root, browsingContext), true);

    await emitTestEvent(root, browsingContext, monitoringEvents);
    is(events.length, 1);

    info(
      "Add another listener for eventemitter.testEvent, using global context"
    );
    const eventsAll = [];
    const onEventAll = (event, data) => eventsAll.push(data.text);
    await root.eventsDispatcher.on(
      "eventemitter.testEvent",
      contextDescriptorAll,
      onEventAll
    );
    await emitTestEvent(root, browsingContext, monitoringEvents);
    is(eventsAll.length, 1);
    is(events.length, 2);

    info("Remove the first listener for eventemitter.testEvent");
    await root.eventsDispatcher.off(
      "eventemitter.testEvent",
      contextDescriptor,
      onEvent
    );

    info("Check that we are still subscribed to eventemitter.testEvent");
    is(await isSubscribed(root, browsingContext), true);
    await emitTestEvent(root, browsingContext, monitoringEvents);
    is(eventsAll.length, 2);
    is(events.length, 2);

    await root.eventsDispatcher.off(
      "eventemitter.testEvent",
      contextDescriptorAll,
      onEventAll
    );
    is(await isSubscribed(root, browsingContext), false);
    await emitTestEvent(root, browsingContext, monitoringEvents);
    is(eventsAll.length, 2);
    is(events.length, 2);

    root.destroy();
    gBrowser.removeTab(tab);
  }
);

/**
 * Check that adding and removing first listener for the global context and then
 * for the specific context works as expected.
 */
add_task(
  async function test_global_event_listener_and_then_remove_context_event_listener() {
    const tab = await addTab(
      "https://example.com/document-builder.sjs?html=tab"
    );
    const browsingContext = tab.linkedBrowser.browsingContext;
    const contextDescriptor = {
      type: ContextDescriptorType.TopBrowsingContext,
      id: browsingContext.browserId,
    };
    const contextDescriptorAll = {
      type: ContextDescriptorType.All,
    };

    const root = createRootMessageHandler("session-id-event");
    const monitoringEvents = await setupEventMonitoring(root);
    await emitTestEvent(root, browsingContext, monitoringEvents);
    is(await isSubscribed(root, browsingContext), false);

    info("Add an listener for eventemitter.testEvent");
    const events = [];
    const onEvent = (event, data) => events.push(data.text);
    await root.eventsDispatcher.on(
      "eventemitter.testEvent",
      contextDescriptor,
      onEvent
    );
    is(await isSubscribed(root, browsingContext), true);

    await emitTestEvent(root, browsingContext, monitoringEvents);
    is(events.length, 1);

    info(
      "Add another listener for eventemitter.testEvent, using global context"
    );
    const eventsAll = [];
    const onEventAll = (event, data) => eventsAll.push(data.text);
    await root.eventsDispatcher.on(
      "eventemitter.testEvent",
      contextDescriptorAll,
      onEventAll
    );
    await emitTestEvent(root, browsingContext, monitoringEvents);
    is(eventsAll.length, 1);
    is(events.length, 2);

    info("Remove the global listener for eventemitter.testEvent");
    await root.eventsDispatcher.off(
      "eventemitter.testEvent",
      contextDescriptorAll,
      onEventAll
    );

    info(
      "Check that we are still subscribed to eventemitter.testEvent for the specific context"
    );
    is(await isSubscribed(root, browsingContext), true);
    await emitTestEvent(root, browsingContext, monitoringEvents);
    is(eventsAll.length, 1);
    is(events.length, 3);

    await root.eventsDispatcher.off(
      "eventemitter.testEvent",
      contextDescriptor,
      onEvent
    );

    is(await isSubscribed(root, browsingContext), false);
    await emitTestEvent(root, browsingContext, monitoringEvents);
    is(eventsAll.length, 1);
    is(events.length, 3);

    root.destroy();
    gBrowser.removeTab(tab);
  }
);

async function setupEventMonitoring(root) {
  const monitoringEvents = [];
  const onMonitoringEvent = (event, data) => monitoringEvents.push(data.text);
  root.on("eventemitter.monitoringEvent", onMonitoringEvent);

  registerCleanupFunction(() =>
    root.off("eventemitter.monitoringEvent", onMonitoringEvent)
  );

  return monitoringEvents;
}

async function emitTestEvent(root, browsingContext, monitoringEvents) {
  const count = monitoringEvents.length;
  info("Call eventemitter.emitTestEvent");
  await root.handleCommand({
    moduleName: "eventemitter",
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
  info("Call eventemitter.isSubscribed");
  return root.handleCommand({
    moduleName: "eventemitter",
    commandName: "isSubscribed",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
  });
}
