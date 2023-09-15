/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RootMessageHandlerRegistry } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandlerRegistry.sys.mjs"
);
const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);

/**
 * Test that events can be intercepted in the windowglobal-in-root layer.
 */
add_task(async function test_intercepted_event() {
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContext = tab.linkedBrowser.browsingContext;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-intercepted_event"
  );

  const onInterceptedEvent = rootMessageHandler.once(
    "event.testEventWithInterception"
  );
  rootMessageHandler.handleCommand({
    moduleName: "event",
    commandName: "testEmitEventWithInterception",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
  });

  const interceptedEvent = await onInterceptedEvent;
  is(
    interceptedEvent.additionalInformation,
    "information added through interception",
    "Intercepted event contained additional information"
  );

  rootMessageHandler.destroy();
  gBrowser.removeTab(tab);
});

/**
 * Test that events can be canceled in the windowglobal-in-root layer.
 */
add_task(async function test_cancelable_event() {
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContext = tab.linkedBrowser.browsingContext;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-cancelable_event"
  );

  const cancelableEvents = [];
  const onCancelableEvent = (name, event) => cancelableEvents.push(event);
  rootMessageHandler.on(
    "event.testEventCancelableWithInterception",
    onCancelableEvent
  );

  // Emit an event that should be canceled in the windowglobal-in-root layer.
  // Note that `shouldCancel` is only something supported for this test event,
  // and not a general message handler mechanism to cancel events.
  await rootMessageHandler.handleCommand({
    moduleName: "event",
    commandName: "testEmitEventCancelableWithInterception",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
    params: {
      shouldCancel: true,
    },
  });

  is(cancelableEvents.length, 0, "No event was received");

  // Emit another event which should not be canceled (shouldCancel: false).
  await rootMessageHandler.handleCommand({
    moduleName: "event",
    commandName: "testEmitEventCancelableWithInterception",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
    params: {
      shouldCancel: false,
    },
  });

  await TestUtils.waitForCondition(() => cancelableEvents.length == 1);
  is(cancelableEvents[0].shouldCancel, false, "Expected event was received");

  rootMessageHandler.off(
    "event.testEventCancelableWithInterception",
    onCancelableEvent
  );
  rootMessageHandler.destroy();
  gBrowser.removeTab(tab);
});
