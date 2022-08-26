/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that the window-global-handler-created event gets emitted for each
 * individual frame's browsing context.
 */
add_task(async function test_windowGlobalHandlerCreated() {
  const events = [];

  const rootMessageHandler = createRootMessageHandler(
    "session-id-event_with_frames"
  );

  info("Add a new session data item to get window global handlers created");
  await rootMessageHandler.addSessionData({
    moduleName: "command",
    category: "browser_session_data_browser_element",
    contextDescriptor: {
      type: ContextDescriptorType.All,
    },
    values: [true],
  });

  const onEvent = (evtName, wrappedEvt) => {
    if (wrappedEvt.name === "window-global-handler-created") {
      console.info(`Received event for context ${wrappedEvt.data.contextId}`);
      events.push(wrappedEvt.data);
    }
  };
  rootMessageHandler.on("message-handler-event", onEvent);

  info("Navigate the initial tab to the test URL");
  const browser = gBrowser.selectedTab.linkedBrowser;
  await loadURL(browser, createTestMarkupWithFrames());

  const contexts = browser.browsingContext.getAllBrowsingContextsInSubtree();
  is(contexts.length, 4, "Test tab has 3 children contexts (4 in total)");

  // Wait for all the events
  await TestUtils.waitForCondition(() => events.length >= 4);

  for (const context of contexts) {
    const contextEvents = events.filter(evt => {
      return (
        evt.contextId === context.id &&
        evt.innerWindowId === context.currentWindowGlobal.innerWindowId
      );
    });
    is(contextEvents.length, 1, `Found event for context ${context.id}`);
  }

  rootMessageHandler.off("message-handler-event", onEvent);
  rootMessageHandler.destroy();
});
