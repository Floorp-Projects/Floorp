/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported withHandlingUserInput */

ChromeUtils.import("resource://gre/modules/MessageChannel.jsm");

let extensionHandlers = new WeakSet();

function handlingUserInputFrameScript() {
  /* globals content */
  ChromeUtils.import("resource://gre/modules/MessageChannel.jsm");

  let handle;
  MessageChannel.addListener(this, "ExtensionTest:HandleUserInput", {
    receiveMessage({name, data}) {
      if (data) {
        handle = content.windowUtils.setHandlingUserInput(true);
      } else if (handle) {
        handle.destruct();
        handle = null;
      }
    },
  });
}

async function withHandlingUserInput(extension, fn) {
  let {messageManager} = extension.extension.groupFrameLoader;

  if (!extensionHandlers.has(extension)) {
    messageManager.loadFrameScript(`data:,(${handlingUserInputFrameScript}).call(this)`, false, true);
    extensionHandlers.add(extension);
  }

  await MessageChannel.sendMessage(messageManager, "ExtensionTest:HandleUserInput", true);
  await fn();
  await MessageChannel.sendMessage(messageManager, "ExtensionTest:HandleUserInput", false);
}
