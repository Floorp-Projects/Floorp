/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MessageHandlerRegistry } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.sys.mjs"
);
const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);

add_task(async function test_messageHandlerRegistry_API() {
  const sessionId = 1;
  const type = RootMessageHandler.type;

  const rootMessageHandlerRegistry = new MessageHandlerRegistry(type);

  const rootMessageHandler =
    rootMessageHandlerRegistry.getOrCreateMessageHandler(sessionId);
  ok(rootMessageHandler, "Valid ROOT MessageHandler created");

  const contextId = rootMessageHandler.contextId;
  ok(contextId, "ROOT MessageHandler has a valid contextId");

  is(
    rootMessageHandler,
    rootMessageHandlerRegistry.getExistingMessageHandler(sessionId),
    "ROOT MessageHandler can be retrieved from the registry"
  );

  rootMessageHandler.destroy();
  ok(
    !rootMessageHandlerRegistry.getExistingMessageHandler(sessionId),
    "Destroyed ROOT MessageHandler is no longer returned by the Registry"
  );
});
