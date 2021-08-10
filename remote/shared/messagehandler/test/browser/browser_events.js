/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MessageHandlerRegistry } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.jsm"
);
const { RootMessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm"
);
const { WindowGlobalMessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.jsm"
);

/**
 * Emit an event from a WindowGlobal module triggered by a specific command.
 * Check that the event is emitted on the RootMessageHandler as well as on
 * the parent process MessageHandlerRegistry.
 */
add_task(async function test_event() {
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "http://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContext = tab.linkedBrowser.browsingContext;

  const rootMessageHandler = createRootMessageHandler("session-id-event");

  const onTestEvent = rootMessageHandler.once("message-handler-event");
  // MessageHandlerRegistry should forward all the message-handler-events.
  const onRegistryEvent = MessageHandlerRegistry.once(
    "message-handler-registry-event"
  );

  callTestEmitEvent(rootMessageHandler, browsingContext.id);

  const messageHandlerEvent = await onTestEvent;
  is(
    messageHandlerEvent.method,
    "event.testEvent",
    "Received event.testEvent on the ROOT MessageHandler"
  );
  is(
    messageHandlerEvent.params.text,
    `event from ${browsingContext.id}`,
    "Received the expected data in testEvent"
  );
  const registryEvent = await onRegistryEvent;
  is(
    registryEvent,
    messageHandlerEvent,
    "The event forwarded by the MessageHandlerRegistry is identical to the MessageHandler event"
  );

  rootMessageHandler.destroy();
  gBrowser.removeTab(tab);
});

/**
 * Emit an event from a Root module triggered by a specific command.
 * Check that the event is emitted on the RootMessageHandler.
 */
add_task(async function test_root_event() {
  const rootMessageHandler = createRootMessageHandler("session-id-root_event");

  const onTestEvent = rootMessageHandler.once("message-handler-event");
  rootMessageHandler.handleCommand({
    moduleName: "event",
    commandName: "testEmitRootEvent",
    destination: {
      type: RootMessageHandler.type,
    },
  });

  const { method, params } = await onTestEvent;
  is(
    method,
    "event.testRootEvent",
    "Received event.testRootEvent on the ROOT MessageHandler"
  );
  is(
    params.text,
    "event from root",
    "Received the expected payload in testRootEvent"
  );

  rootMessageHandler.destroy();
});

/**
 * Emit an event from a windowglobal-in-root module triggered by a specific command.
 * Check that the event is emitted on the RootMessageHandler.
 */
add_task(async function test_windowglobal_in_root_event() {
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "http://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContext = tab.linkedBrowser.browsingContext;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-windowglobal_in_root_event"
  );

  const onTestEvent = rootMessageHandler.once("message-handler-event");
  rootMessageHandler.handleCommand({
    moduleName: "event",
    commandName: "testEmitWindowGlobalInRootEvent",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
  });

  const { method, params } = await onTestEvent;
  is(
    method,
    "event.testWindowGlobalInRootEvent",
    "Received event.testWindowGlobalInRoot on the ROOT MessageHandler"
  );
  is(
    params.text,
    `windowglobal-in-root event for ${browsingContext.id}`,
    "Received the expected payload in testWindowGlobalInRoot"
  );

  rootMessageHandler.destroy();
  gBrowser.removeTab(tab);
});

/**
 * Emit an event from a windowglobal module, but from 2 different sessions.
 * Check that the event is emitted by the corresponding RootMessageHandler as
 * well as by the parent process MessageHandlerRegistry.
 */
add_task(async function test_event_multisession() {
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "http://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContextId = tab.linkedBrowser.browsingContext.id;

  const root1 = createRootMessageHandler("session-id-event_multisession-1");
  let root1Events = 0;
  const onRoot1Event = function(evtName, wrappedEvt) {
    if (wrappedEvt.method === "event.testEvent") {
      root1Events++;
    }
  };
  root1.on("message-handler-event", onRoot1Event);

  const root2 = createRootMessageHandler("session-id-event_multisession-2");
  let root2Events = 0;
  const onRoot2Event = function(evtName, wrappedEvt) {
    if (wrappedEvt.method === "event.testEvent") {
      root2Events++;
    }
  };
  root2.on("message-handler-event", onRoot2Event);

  let registryEvents = 0;
  const onRegistryEvent = function(evtName, wrappedEvt) {
    if (wrappedEvt.method === "event.testEvent") {
      registryEvents++;
    }
  };
  MessageHandlerRegistry.on("message-handler-registry-event", onRegistryEvent);

  callTestEmitEvent(root1, browsingContextId);
  callTestEmitEvent(root2, browsingContextId);

  info("Wait for root1 event to be received");
  await TestUtils.waitForCondition(() => root1Events === 1);
  info("Wait for root2 event to be received");
  await TestUtils.waitForCondition(() => root2Events === 1);

  await TestUtils.waitForTick();
  is(root1Events, 1, "Session 1 only received 1 event");
  is(root2Events, 1, "Session 2 only received 1 event");
  is(
    registryEvents,
    2,
    "MessageHandlerRegistry forwarded events from both sessions"
  );

  root1.off("message-handler-event", onRoot1Event);
  root2.off("message-handler-event", onRoot2Event);
  MessageHandlerRegistry.off("message-handler-registry-event", onRegistryEvent);
  root1.destroy();
  root2.destroy();
  gBrowser.removeTab(tab);
});

/**
 * Test that events can be emitted from individual frame contexts and that
 * events going through a shared content process MessageHandlerRegistry are not
 * duplicated.
 */
add_task(async function test_event_with_frames() {
  info("Navigate the initial tab to the test URL");
  const tab = gBrowser.selectedTab;
  await loadURL(tab.linkedBrowser, createTestMarkupWithFrames());

  const contexts = tab.linkedBrowser.browsingContext.getAllBrowsingContextsInSubtree();
  is(contexts.length, 4, "Test tab has 3 children contexts (4 in total)");

  const rootMessageHandler = createRootMessageHandler(
    "session-id-event_with_frames"
  );

  let rootEvents = [];
  const onRootEvent = function(evtName, wrappedEvt) {
    if (wrappedEvt.method === "event.testEvent") {
      rootEvents.push(wrappedEvt.params.text);
    }
  };
  rootMessageHandler.on("message-handler-event", onRootEvent);

  for (const context of contexts) {
    callTestEmitEvent(rootMessageHandler, context.id);
    info("Wait for root event to be received");
    await TestUtils.waitForCondition(() =>
      rootEvents.includes(`event from ${context.id}`)
    );
  }

  info("Wait for a bit and check that we did not receive duplicated events");
  await TestUtils.waitForTick();
  is(rootEvents.length, 4, "Only received 4 events");

  rootMessageHandler.off("message-handler-event", onRootEvent);
  rootMessageHandler.destroy();
});

function callTestEmitEvent(rootMessageHandler, browsingContextId) {
  rootMessageHandler.handleCommand({
    moduleName: "event",
    commandName: "testEmitEvent",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });
}
