/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RootMessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm"
);
const { WindowGlobalMessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.jsm"
);

// Test calling methods only implemented in the root version of a module.
add_task(async function test_rootModule_command() {
  const rootMessageHandler = createRootMessageHandler("session-id-rootModule");
  const rootValue = await rootMessageHandler.handleCommand({
    moduleName: "command",
    commandName: "testRootModule",
    destination: {
      type: RootMessageHandler.type,
    },
  });

  is(
    rootValue,
    "root-value",
    "Retrieved the expected value from testRootModule"
  );

  rootMessageHandler.destroy();
});

// Test calling methods only implemented in the windowglobal-in-root version of
// a module.
add_task(async function test_windowglobalInRootModule_command() {
  const browsingContextId = gBrowser.selectedBrowser.browsingContext.id;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-windowglobalInRootModule"
  );
  const interceptedValue = await rootMessageHandler.handleCommand({
    moduleName: "command",
    commandName: "testInterceptModule",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });

  is(
    interceptedValue,
    "intercepted-value",
    "Retrieved the expected value from testInterceptModule"
  );

  rootMessageHandler.destroy();
});

// Test calling methods only implemented in the windowglobal version of a
// module.
add_task(async function test_windowglobalModule_command() {
  const browsingContextId = gBrowser.selectedBrowser.browsingContext.id;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-windowglobalModule"
  );
  const windowGlobalValue = await rootMessageHandler.handleCommand({
    moduleName: "command",
    commandName: "testWindowGlobalModule",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });

  is(
    windowGlobalValue,
    "windowglobal-value",
    "Retrieved the expected value from testWindowGlobalModule"
  );

  rootMessageHandler.destroy();
});

// Test calling a method on a module which is only available in the "windowglobal"
// folder. This will check that the MessageHandler/ModuleCache correctly moves
// on to the next layer when no implementation can be found in the root layer.
add_task(async function test_windowglobalOnlyModule_command() {
  const browsingContextId = gBrowser.selectedBrowser.browsingContext.id;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-windowglobalOnlyModule"
  );
  const windowGlobalOnlyValue = await rootMessageHandler.handleCommand({
    moduleName: "commandwindowglobalonly",
    commandName: "testOnlyInWindowGlobal",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });

  is(
    windowGlobalOnlyValue,
    "only-in-windowglobal",
    "Retrieved the expected value from testOnlyInWindowGlobal"
  );

  rootMessageHandler.destroy();
});

// Try to create 2 sessions which will both set values in individual modules
// via a command `testSetValue`, and then retrieve the values via another
// command `testGetValue`.
// This will ensure that different sessions use different module instances.
add_task(async function test_multisession() {
  const browsingContextId = gBrowser.selectedBrowser.browsingContext.id;

  const rootMessageHandler1 = createRootMessageHandler(
    "session-id-multisession-1"
  );
  const rootMessageHandler2 = createRootMessageHandler(
    "session-id-multisession-2"
  );

  info("Set value for session 1");
  await rootMessageHandler1.handleCommand({
    moduleName: "command",
    commandName: "testSetValue",
    params: { value: "session1-value" },
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });

  info("Set value for session 2");
  await rootMessageHandler2.handleCommand({
    moduleName: "command",
    commandName: "testSetValue",
    params: { value: "session2-value" },
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });

  const session1Value = await rootMessageHandler1.handleCommand({
    moduleName: "command",
    commandName: "testGetValue",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });

  is(
    session1Value,
    "session1-value",
    "Retrieved the expected value for session 1"
  );

  const session2Value = await rootMessageHandler2.handleCommand({
    moduleName: "command",
    commandName: "testGetValue",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });

  is(
    session2Value,
    "session2-value",
    "Retrieved the expected value for session 2"
  );

  rootMessageHandler1.destroy();
  rootMessageHandler2.destroy();
});

// Test calling a method from the windowglobal-in-root module which will
// internally forward to the windowglobal module and will return a composite
// result built both in parent and content process.
add_task(async function test_forwarding_command() {
  const browsingContextId = gBrowser.selectedBrowser.browsingContext.id;

  const rootMessageHandler = createRootMessageHandler("session-id-forwarding");
  const interceptAndForwardValue = await rootMessageHandler.handleCommand({
    moduleName: "command",
    commandName: "testInterceptAndForwardModule",
    params: { id: "value" },
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });

  is(
    interceptAndForwardValue,
    "intercepted-and-forward+forward-to-windowglobal-value",
    "Retrieved the expected value from testInterceptAndForwardModule"
  );

  rootMessageHandler.destroy();
});
