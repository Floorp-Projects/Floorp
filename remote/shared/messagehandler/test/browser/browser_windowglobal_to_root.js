/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);

add_task(async function test_windowGlobal_to_root_command() {
  const browsingContextId = gBrowser.selectedBrowser.browsingContext.id;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-windowglobal-to-rootModule"
  );

  for (const commandName of [
    "testHandleCommandToRoot",
    "testSendRootCommand",
  ]) {
    const valueFromRoot = await rootMessageHandler.handleCommand({
      moduleName: "windowglobaltoroot",
      commandName,
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: browsingContextId,
      },
    });

    is(
      valueFromRoot,
      "root-value-called-from-windowglobal",
      "Retrieved the expected value from windowglobaltoroot using " +
        commandName
    );
  }

  rootMessageHandler.destroy();
});
