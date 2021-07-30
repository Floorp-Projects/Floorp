/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function createRootMessageHandler(sessionId) {
  const { MessageHandlerRegistry } = ChromeUtils.import(
    "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.jsm"
  );
  const { RootMessageHandler } = ChromeUtils.import(
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm"
  );
  return MessageHandlerRegistry.getOrCreateMessageHandler(
    sessionId,
    RootMessageHandler.type
  );
}
