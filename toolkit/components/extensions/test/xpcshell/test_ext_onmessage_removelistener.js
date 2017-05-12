/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function backgroundScript() {
  function listener() {
    browser.test.notifyFail("listener should not be invoked");
  }

  browser.runtime.onMessage.addListener(listener);
  browser.runtime.onMessage.removeListener(listener);
  browser.runtime.sendMessage("hello");

  // Make sure that, if we somehow fail to remove the listener, then we'll run
  // the listener before the test is marked as passing.
  setTimeout(function() {
    browser.test.notifyPass("onmessage_removelistener");
  }, 0);
}

let extensionData = {
  background: backgroundScript,
};

add_task(async function test_contentscript() {
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitFinish("onmessage_removelistener");
  await extension.unload();
});
