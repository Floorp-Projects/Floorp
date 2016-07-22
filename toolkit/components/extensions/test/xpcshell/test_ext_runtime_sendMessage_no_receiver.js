/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_sendMessage_without_listener() {
  function background() {
    browser.runtime.sendMessage("msg").then(reply => {
      browser.test.assertEq(undefined, reply);
      browser.test.notifyFail("Did not expect a reply to sendMessage");
    }, error => {
      browser.test.assertEq("Could not establish connection. Receiving end does not exist.", error.message);
      browser.test.notifyPass("sendMessage callback was invoked");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({background});
  yield extension.startup();

  yield extension.awaitFinish("sendMessage callback was invoked");

  yield extension.unload();
});
