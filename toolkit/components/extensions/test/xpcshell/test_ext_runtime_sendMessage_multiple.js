/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Regression test for bug 1655624: When there are multiple onMessage receivers
// that both handle the response asynchronously, destroying the context of one
// recipient should not prevent the other recipient from sending a reply.
add_task(async function onMessage_ignores_destroyed_contexts() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.onMessage.addListener(async msg => {
        if (msg !== "startTest") {
          return;
        }
        try {
          let res = await browser.runtime.sendMessage("msg_from_bg");
          browser.test.assertEq(0, res, "Result from onMessage");
          browser.test.notifyPass("handled_onMessage");
        } catch (e) {
          browser.test.fail(`Unexpected error: ${e.message} :: ${e.stack}`);
          browser.test.notifyFail("handled_onMessage");
        }
      });
    },
    files: {
      "tab.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <script src="tab.js"></script>
      `,
      "tab.js": () => {
        let where = location.search.slice(1);
        let resolveOnMessage;
        browser.runtime.onMessage.addListener(async msg => {
          browser.test.assertEq("msg_from_bg", msg, `onMessage at ${where}`);
          browser.test.sendMessage(`received:${where}`);
          return new Promise(resolve => {
            resolveOnMessage = resolve;
          });
        });
        browser.test.onMessage.addListener(msg => {
          if (msg === `resolveOnMessage:${where}`) {
            resolveOnMessage(0);
          }
        });
      },
    },
  });
  await extension.startup();
  let tabToCloseEarly = await ExtensionTestUtils.loadContentPage(
    `moz-extension://${extension.uuid}/tab.html?tabToCloseEarly`,
    { extension }
  );
  let tabToRespond = await ExtensionTestUtils.loadContentPage(
    `moz-extension://${extension.uuid}/tab.html?tabToRespond`,
    { extension }
  );
  extension.sendMessage("startTest");
  await Promise.all([
    extension.awaitMessage("received:tabToCloseEarly"),
    extension.awaitMessage("received:tabToRespond"),
  ]);
  await tabToCloseEarly.close();
  extension.sendMessage("resolveOnMessage:tabToRespond");
  await extension.awaitFinish("handled_onMessage");
  await tabToRespond.close();
  await extension.unload();
});
