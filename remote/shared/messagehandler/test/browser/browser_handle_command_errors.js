/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { WindowGlobalMessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.jsm"
);

// Check that errors from WindowGlobal modules can be caught by the consumer
// of the RootMessageHandler.
add_task(async function test_module_error() {
  const browsingContextId = gBrowser.selectedBrowser.browsingContext.id;

  const rootMessageHandler = createRootMessageHandler("session-id-error");

  info("Call a module method which will throw");
  try {
    await rootMessageHandler.handleCommand({
      moduleName: "commandwindowglobalonly",
      commandName: "testError",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: browsingContextId,
      },
    });
    ok(false, "Error from window global module was not caught");
  } catch (e) {
    ok(true, "Error from window global module caught");
  }

  rootMessageHandler.destroy();
});

// Check that sending commands to incorrect destinations creates an error which
// can be caught by the consumer of the RootMessageHandler.
add_task(async function test_destination_error() {
  const rootMessageHandler = createRootMessageHandler("session-id-error");

  info("Call a valid module method, but on a non-existent browsing context id");
  try {
    const fakeBrowsingContextId = -1;
    ok(
      !BrowsingContext.get(fakeBrowsingContextId),
      "No browsing context matches fakeBrowsingContextId"
    );
    await rootMessageHandler.handleCommand({
      moduleName: "commandwindowglobalonly",
      commandName: "testOnlyInWindowGlobal",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: fakeBrowsingContextId,
      },
    });
    ok(false, "Incorrect destination error was not caught");
  } catch (e) {
    ok(true, "Incorrect destination error was caught");
  }

  rootMessageHandler.destroy();
});
