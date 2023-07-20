/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MessageHandlerRegistry } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.sys.mjs"
);
const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);

// Check that a functional navigation manager is available on the
// RootMessageHandler.
add_task(async function test_navigationManager() {
  const sessionId = "navigationManager-test";
  const type = RootMessageHandler.type;

  const rootMessageHandlerRegistry = new MessageHandlerRegistry(type);

  const rootMessageHandler =
    rootMessageHandlerRegistry.getOrCreateMessageHandler(sessionId);

  const navigationManager = rootMessageHandler.navigationManager;
  ok(!!navigationManager, "ROOT MessageHandler provides a navigation manager");

  const events = [];
  const onEvent = (name, data) => events.push({ name, data });
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  info("Check the navigation manager monitors navigations");

  const testUrl = "https://example.com/document-builder.sjs?html=test";
  const tab1 = BrowserTestUtils.addTab(gBrowser, testUrl);
  const contentBrowser1 = tab1.linkedBrowser;
  await BrowserTestUtils.browserLoaded(contentBrowser1);

  const navigation = navigationManager.getNavigationForBrowsingContext(
    contentBrowser1.browsingContext
  );
  is(navigation.url, testUrl, "Navigation has the expected URL");

  is(events.length, 2, "Received 2 navigation events");
  is(events[0].name, "navigation-started");
  is(events[1].name, "navigation-stopped");

  info(
    "Check the navigation manager is destroyed after destroying the message handler"
  );
  rootMessageHandler.destroy();
  const otherUrl = "https://example.com/document-builder.sjs?html=other";
  const tab2 = BrowserTestUtils.addTab(gBrowser, otherUrl);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
  is(events.length, 2, "No new navigation event received");

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
});
