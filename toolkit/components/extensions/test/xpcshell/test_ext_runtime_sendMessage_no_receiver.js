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
  let extensionData = {
    background,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  yield extension.startup();

  yield extension.awaitFinish("sendMessage callback was invoked");

  yield extension.unload();
});

add_task(function* test_chrome_sendMessage_without_listener() {
  function background() {
    /* globals chrome */
    browser.test.assertEq(null, chrome.runtime.lastError, "no lastError before call");
    let retval = chrome.runtime.sendMessage("msg");
    browser.test.assertEq(null, chrome.runtime.lastError, "no lastError after call");
    browser.test.assertEq(undefined, retval, "return value of chrome.runtime.sendMessage without callback");

    let isAsyncCall = false;
    retval = chrome.runtime.sendMessage("msg", reply => {
      browser.test.assertEq(undefined, reply, "no reply");
      browser.test.assertTrue(isAsyncCall, "chrome.runtime.sendMessage's callback must be called asynchronously");
      browser.test.assertEq(undefined, retval, "return value of chrome.runtime.sendMessage with callback");
      browser.test.assertEq("Could not establish connection. Receiving end does not exist.", chrome.runtime.lastError.message);
      browser.test.notifyPass("finished chrome.runtime.sendMessage");
    });
    isAsyncCall = true;
  }
  let extensionData = {
    background,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  yield extension.startup();

  yield extension.awaitFinish("finished chrome.runtime.sendMessage");

  yield extension.unload();
});
