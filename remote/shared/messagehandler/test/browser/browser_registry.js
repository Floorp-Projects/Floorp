/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MessageHandlerRegistry } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.jsm"
);
const { RootMessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm"
);

add_task(async function test_messageHandlerRegistry_API() {
  const sessionId = 1;
  const type = RootMessageHandler.type;

  const rootMessageHandler = MessageHandlerRegistry.getOrCreateMessageHandler(
    sessionId,
    type
  );
  ok(rootMessageHandler, "Valid ROOT MessageHandler created");

  const contextId = rootMessageHandler.contextId;
  ok(contextId, "ROOT MessageHandler has a valid contextId");

  is(
    rootMessageHandler,
    MessageHandlerRegistry.getExistingMessageHandler(
      sessionId,
      type,
      contextId
    ),
    "ROOT MessageHandler can be retrieved from the registry"
  );

  rootMessageHandler.destroy();
  ok(
    !MessageHandlerRegistry.getExistingMessageHandler(
      sessionId,
      type,
      contextId
    ),
    "Destroyed ROOT MessageHandler is no longer returned by the Registry"
  );
});
