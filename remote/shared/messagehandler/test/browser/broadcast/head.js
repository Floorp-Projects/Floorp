/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/shared/messagehandler/test/browser/head.js",
  this
);

/**
 * Broadcast the provided method to WindowGlobal contexts on a MessageHandler
 * network.
 * Returns a promise which will resolve the result of the command broadcast.
 *
 * @param {String} module
 *     The name of the module implementing the command to broadcast.
 * @param {String} command
 *     The name of the command to broadcast.
 * @param {Object} params
 *     The parameters for the command.
 * @param {ContextDescriptor} contextDescriptor
 *     The context descriptor to use for this broadcast
 * @param {RootMessageHandler} rootMessageHandler
 *     The root of the MessageHandler network.
 * @return {Promise.<Array>}
 *     Promise which resolves an array where each item is the result of the
 *     command handled by an individual context.
 */
function sendTestBroadcastCommand(
  module,
  command,
  params,
  contextDescriptor,
  rootMessageHandler
) {
  const { WindowGlobalMessageHandler } = ChromeUtils.import(
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.jsm"
  );

  info("Send a test broadcast command");
  return rootMessageHandler.handleCommand({
    moduleName: module,
    commandName: command,
    params,
    destination: {
      contextDescriptor,
      type: WindowGlobalMessageHandler.type,
    },
  });
}
