/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function() {
  const ID1 = "sendMessage1@tests.mozilla.org";
  const ID2 = "sendMessage2@tests.mozilla.org";

  let extension1 = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.onMessage.addListener((...args) => {
        browser.runtime.sendMessage(...args);
      });

      let frame = document.createElement("iframe");
      frame.src = "page.html";
      document.body.appendChild(frame);
    },
    manifest: { applications: { gecko: { id: ID1 } } },
    files: {
      "page.js": function () {
        browser.runtime.onMessage.addListener((msg, sender) => {
          browser.test.sendMessage("received-page", { msg, sender });
        });
        // Let them know we're done loading the page.
        browser.test.sendMessage("page-ready");
      },
      "page.html": `<!DOCTYPE html><meta charset="utf-8"><script src="page.js"></script>`,
    },
  });

  let extension2 = ExtensionTestUtils.loadExtension({
    background() {
      browser.runtime.onMessageExternal.addListener((msg, sender) => {
        browser.test.sendMessage("received-external", { msg, sender });
      });
    },
    manifest: { applications: { gecko: { id: ID2 } } },
  });

  await Promise.all([extension1.startup(), extension2.startup()]);
  await extension1.awaitMessage("page-ready");

  // Check that a message was sent within extension1.
  async function checkLocalMessage(msg) {
    let result = await extension1.awaitMessage("received-page");
    deepEqual(result.msg, msg, "Received internal message");
    equal(result.sender.id, ID1, "Received correct sender id");
  }

  // Check that a message was sent from extension1 to extension2.
  async function checkRemoteMessage(msg) {
    let result = await extension2.awaitMessage("received-external");
    deepEqual(result.msg, msg, "Received cross-extension message");
    equal(result.sender.id, ID1, "Received correct sender id");
  }

  // sendMessage() takes 3 arguments:
  //  optional extensionID
  //  mandatory message
  //  optional options
  // Due to this insane design we parse its arguments manually.  This
  // test is meant to cover all the combinations.

  // A single null or undefined argument is allowed, and represents the message
  extension1.sendMessage(null);
  await checkLocalMessage(null);

  // With one argument, it must be just the message
  extension1.sendMessage("message");
  await checkLocalMessage("message");

  // With two arguments, these cases should be treated as (extensionID, message)
  extension1.sendMessage(ID2, "message");
  await checkRemoteMessage("message");

  extension1.sendMessage(ID2, { msg: "message" });
  await checkRemoteMessage({ msg: "message" });

  // And these should be (message, options)
  extension1.sendMessage("message", {});
  await checkLocalMessage("message");

  // or (message, non-callback), pick your poison
  extension1.sendMessage("message", undefined);
  await checkLocalMessage("message");

  // With three arguments, we send a cross-extension message
  extension1.sendMessage(ID2, "message", {});
  await checkRemoteMessage("message");

  // Even when the last one is null or undefined
  extension1.sendMessage(ID2, "message", undefined);
  await checkRemoteMessage("message");

  // The four params case is unambigous, so we allow null as a (non-) callback
  extension1.sendMessage(ID2, "message", {}, null);
  await checkRemoteMessage("message");

  await Promise.all([extension1.unload(), extension2.unload()]);
});
