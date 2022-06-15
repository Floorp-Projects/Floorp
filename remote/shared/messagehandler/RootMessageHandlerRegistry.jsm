/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["RootMessageHandlerRegistry"];

const { MessageHandlerRegistry } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.jsm"
);
const { RootMessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm"
);

/**
 * In the parent process, only one Root MessageHandlerRegistry should ever be
 * created. All consumers can safely use this singleton to retrieve the Root
 * registry and from there either create or retrieve Root MessageHandler
 * instances for a specific session.
 */
var RootMessageHandlerRegistry = new MessageHandlerRegistry(
  RootMessageHandler.type
);
