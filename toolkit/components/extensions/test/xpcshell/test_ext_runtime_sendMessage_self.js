/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

add_task(
  async function test_sendMessage_connect_to_self_should_not_trigger_onMessage_onConnect() {
    async function background() {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.assertEq("msg from child", msg);
        browser.test.notifyPass(
          "sendMessage did not call same-frame onMessage"
        );
      });

      browser.test.onMessage.addListener(msg => {
        browser.test.assertEq(
          "sendMessage with a listener in another frame",
          msg
        );
        browser.runtime.sendMessage("should only reach another frame");
      });

      await browser.test.assertRejects(
        browser.runtime.sendMessage("should not trigger same-frame onMessage"),
        "Could not establish connection. Receiving end does not exist."
      );

      browser.runtime.onConnect.addListener(port => {
        browser.test.fail("Should not receive runtime.onConnect from self");
      });

      await new Promise(resolve => {
        let port = browser.runtime.connect();
        port.onDisconnect.addListener(() => {
          browser.test.assertEq(
            "Could not establish connection. Receiving end does not exist.",
            port.error.message
          );
          resolve();
        });
      });

      let anotherFrame = document.createElement("iframe");
      anotherFrame.src = browser.extension.getURL("extensionpage.html");
      document.body.appendChild(anotherFrame);
    }

    function lastScript() {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.assertEq("should only reach another frame", msg);
        browser.runtime.sendMessage("msg from child");
      });
      browser.test.sendMessage("sendMessage callback called");
    }

    let extensionData = {
      background,
      files: {
        "lastScript.js": lastScript,
        "extensionpage.html": `<!DOCTYPE html><meta charset="utf-8"><script src="lastScript.js"></script>`,
      },
    };

    let extension = ExtensionTestUtils.loadExtension(extensionData);
    await extension.startup();

    await extension.awaitMessage("sendMessage callback called");
    extension.sendMessage("sendMessage with a listener in another frame");
    await extension.awaitFinish(
      "sendMessage did not call same-frame onMessage"
    );

    await extension.unload();
  }
);
