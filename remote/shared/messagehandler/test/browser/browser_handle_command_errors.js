/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);

// Check that errors from WindowGlobal modules can be caught by the consumer
// of the RootMessageHandler.
add_task(async function test_module_error() {
  const browsingContextId = gBrowser.selectedBrowser.browsingContext.id;

  const rootMessageHandler = createRootMessageHandler("session-id-error");

  info("Call a module method which will throw");

  await Assert.rejects(
    rootMessageHandler.handleCommand({
      moduleName: "commandwindowglobalonly",
      commandName: "testError",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: browsingContextId,
      },
    }),
    err => err.message.includes("error-from-module"),
    "Error from window global module caught"
  );

  rootMessageHandler.destroy();
});

// Check that sending commands to incorrect destinations creates an error which
// can be caught by the consumer of the RootMessageHandler.
add_task(async function test_destination_error() {
  const rootMessageHandler = createRootMessageHandler("session-id-error");

  const fakeBrowsingContextId = -1;
  ok(
    !BrowsingContext.get(fakeBrowsingContextId),
    "No browsing context matches fakeBrowsingContextId"
  );

  info("Call a valid module method, but on a non-existent browsing context id");
  Assert.throws(
    () =>
      rootMessageHandler.handleCommand({
        moduleName: "commandwindowglobalonly",
        commandName: "testOnlyInWindowGlobal",
        destination: {
          type: WindowGlobalMessageHandler.type,
          id: fakeBrowsingContextId,
        },
      }),
    err => err.message == `Unable to find a BrowsingContext for id -1`
  );

  rootMessageHandler.destroy();
});

add_task(async function test_invalid_module_error() {
  const rootMessageHandler = createRootMessageHandler(
    "session-id-missing_module"
  );

  info("Attempt to call a Root module which has a syntax error");
  Assert.throws(
    () =>
      rootMessageHandler.handleCommand({
        moduleName: "invalid",
        commandName: "someMethod",
        destination: {
          type: RootMessageHandler.type,
        },
      }),
    err =>
      err.name === "SyntaxError" &&
      err.message == "expected expression, got ';'"
  );

  rootMessageHandler.destroy();
});

add_task(async function test_missing_root_module_error() {
  const rootMessageHandler = createRootMessageHandler(
    "session-id-missing_module"
  );

  info("Attempt to call a Root module which doesn't exist");
  Assert.throws(
    () =>
      rootMessageHandler.handleCommand({
        moduleName: "missingmodule",
        commandName: "someMethod",
        destination: {
          type: RootMessageHandler.type,
        },
      }),
    err =>
      err.name == "UnsupportedCommandError" &&
      err.message ==
        `missingmodule.someMethod not supported for destination ROOT`
  );

  rootMessageHandler.destroy();
});

add_task(async function test_missing_windowglobal_module_error() {
  const browsingContextId = gBrowser.selectedBrowser.browsingContext.id;
  const rootMessageHandler = createRootMessageHandler(
    "session-id-missing_windowglobal_module"
  );

  info("Attempt to call a WindowGlobal module which doesn't exist");
  Assert.throws(
    () =>
      rootMessageHandler.handleCommand({
        moduleName: "missingmodule",
        commandName: "someMethod",
        destination: {
          type: WindowGlobalMessageHandler.type,
          id: browsingContextId,
        },
      }),
    err =>
      err.name == "UnsupportedCommandError" &&
      err.message ==
        `missingmodule.someMethod not supported for destination WINDOW_GLOBAL`
  );

  rootMessageHandler.destroy();
});

add_task(async function test_missing_root_method_error() {
  const rootMessageHandler = createRootMessageHandler(
    "session-id-missing_root_method"
  );

  info("Attempt to call an invalid method on a Root module");
  Assert.throws(
    () =>
      rootMessageHandler.handleCommand({
        moduleName: "command",
        commandName: "wrongMethod",
        destination: {
          type: RootMessageHandler.type,
        },
      }),
    err =>
      err.name == "UnsupportedCommandError" &&
      err.message == `command.wrongMethod not supported for destination ROOT`
  );

  rootMessageHandler.destroy();
});

add_task(async function test_missing_windowglobal_method_error() {
  const browsingContextId = gBrowser.selectedBrowser.browsingContext.id;
  const rootMessageHandler = createRootMessageHandler(
    "session-id-missing_windowglobal_method"
  );

  info("Attempt to call an invalid method on a WindowGlobal module");
  Assert.throws(
    () =>
      rootMessageHandler.handleCommand({
        moduleName: "commandwindowglobalonly",
        commandName: "wrongMethod",
        destination: {
          type: WindowGlobalMessageHandler.type,
          id: browsingContextId,
        },
      }),
    err =>
      err.name == "UnsupportedCommandError" &&
      err.message ==
        `commandwindowglobalonly.wrongMethod not supported for destination WINDOW_GLOBAL`
  );

  rootMessageHandler.destroy();
});

/**
 * This test checks that even if a command is rerouted to another command after
 * the RootMessageHandler, we still check the new command and log a useful
 * error message.
 *
 * This illustrates why it is important to perform the command check at each
 * layer of the MessageHandler network.
 */
add_task(async function test_missing_intermediary_method_error() {
  const browsingContextId = gBrowser.selectedBrowser.browsingContext.id;
  const rootMessageHandler = createRootMessageHandler(
    "session-id-missing_intermediary_method"
  );

  info(
    "Call a (valid) command that relies on another (missing) command on a WindowGlobal module"
  );
  await Assert.rejects(
    rootMessageHandler.handleCommand({
      moduleName: "commandwindowglobalonly",
      commandName: "testMissingIntermediaryMethod",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: browsingContextId,
      },
    }),
    err =>
      err.name == "UnsupportedCommandError" &&
      err.message ==
        `commandwindowglobalonly.missingMethod not supported for destination WINDOW_GLOBAL`
  );

  rootMessageHandler.destroy();
});
