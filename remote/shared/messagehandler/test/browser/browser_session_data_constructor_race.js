/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE = "https://example.com/document-builder.sjs?html=tab";

/**
 * Check that modules created early for session data are still created with a
 * fully initialized MessageHandler. See Bug 1743083.
 */
add_task(async function () {
  const tab = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContext = tab.linkedBrowser.browsingContext;

  const root = createRootMessageHandler("session-id-event");

  info("Add some session data for the command module");
  await root.addSessionDataItem({
    moduleName: "command",
    category: "testCategory",
    contextDescriptor: contextDescriptorAll,
    values: ["some-value"],
  });

  info("Reload the current tab to create new message handlers and modules");
  await BrowserTestUtils.reloadTab(tab);

  info(
    "Check if the command module was created by the MessageHandler constructor"
  );
  const isCreatedByMessageHandlerConstructor = await root.handleCommand({
    moduleName: "command",
    commandName: "testIsCreatedByMessageHandlerConstructor",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
  });

  is(
    isCreatedByMessageHandlerConstructor,
    false,
    "The command module from session data should not be created by the MessageHandler constructor"
  );
  root.destroy();

  gBrowser.removeTab(tab);
});
