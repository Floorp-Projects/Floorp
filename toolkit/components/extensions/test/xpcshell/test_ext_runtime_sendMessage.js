/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function tabsSendMessageReply() {
  function background() {
    browser.runtime.onMessage.addListener((msg, sender, respond) => {
      if (msg == "respond-now") {
        respond(msg);
      } else if (msg == "respond-soon") {
        setTimeout(() => { respond(msg); }, 0);
        return true;
      } else if (msg == "respond-promise") {
        return Promise.resolve(msg);
      } else if (msg == "respond-never") {
        return undefined;
      } else if (msg == "respond-error") {
        return Promise.reject(new Error(msg));
      } else if (msg == "throw-error") {
        throw new Error(msg);
      }
    });

    browser.runtime.onMessage.addListener((msg, sender, respond) => {
      if (msg == "respond-now") {
        respond("hello");
      } else if (msg == "respond-now-2") {
        respond(msg);
      }
    });

    let childFrame = document.createElement("iframe");
    childFrame.src = "extensionpage.html";
    document.body.appendChild(childFrame);
  }

  function senderScript() {
    Promise.all([
      browser.runtime.sendMessage("respond-now"),
      browser.runtime.sendMessage("respond-now-2"),
      new Promise(resolve => browser.runtime.sendMessage("respond-soon", resolve)),
      browser.runtime.sendMessage("respond-promise"),
      browser.runtime.sendMessage("respond-never"),
      new Promise(resolve => {
        browser.runtime.sendMessage("respond-never", response => { resolve(response); });
      }),

      browser.runtime.sendMessage("respond-error").catch(error => Promise.resolve({error})),
      browser.runtime.sendMessage("throw-error").catch(error => Promise.resolve({error})),
    ]).then(([respondNow, respondNow2, respondSoon, respondPromise, respondNever, respondNever2, respondError, throwError]) => {
      browser.test.assertEq("respond-now", respondNow, "Got the expected immediate response");
      browser.test.assertEq("respond-now-2", respondNow2, "Got the expected immediate response from the second listener");
      browser.test.assertEq("respond-soon", respondSoon, "Got the expected delayed response");
      browser.test.assertEq("respond-promise", respondPromise, "Got the expected promise response");
      browser.test.assertEq(undefined, respondNever, "Got the expected no-response resolution");
      browser.test.assertEq(undefined, respondNever2, "Got the expected no-response resolution");

      browser.test.assertEq("respond-error", respondError.error.message, "Got the expected error response");
      browser.test.assertEq("throw-error", throwError.error.message, "Got the expected thrown error response");

      browser.test.notifyPass("sendMessage");
    }).catch(e => {
      browser.test.fail(`Error: ${e} :: ${e.stack}`);
      browser.test.notifyFail("sendMessage");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    files: {
      "senderScript.js": senderScript,
      "extensionpage.html": `<!DOCTYPE html><meta charset="utf-8"><script src="senderScript.js"></script>`,
    },
  });

  await extension.startup();
  await extension.awaitFinish("sendMessage");
  await extension.unload();
});

add_task(async function tabsSendMessageBlob() {
  function background() {
    browser.runtime.onMessage.addListener(msg => {
      browser.test.assertTrue(msg.blob instanceof Blob, "Message is a blob");
      return Promise.resolve(msg);
    });

    let childFrame = document.createElement("iframe");
    childFrame.src = "extensionpage.html";
    document.body.appendChild(childFrame);
  }

  function senderScript() {
    browser.runtime.sendMessage({blob: new Blob(["hello"])}).then(response => {
      browser.test.assertTrue(response.blob instanceof Blob, "Response is a blob");
      browser.test.notifyPass("sendBlob");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    files: {
      "senderScript.js": senderScript,
      "extensionpage.html": `<!DOCTYPE html><meta charset="utf-8"><script src="senderScript.js"></script>`,
    },
  });

  await extension.startup();
  await extension.awaitFinish("sendBlob");
  await extension.unload();
});

add_task(async function sendMessageResponseGC() {
  function background() {
    let savedResolve, savedRespond;

    browser.runtime.onMessage.addListener((msg, _, respond) => {
      browser.test.log(`Got request: ${msg}`);
      switch (msg) {
        case "ping":
          respond("pong");
          return;

        case "promise-save":
          return new Promise(resolve => {
            savedResolve = resolve;
          });
        case "promise-resolve":
          savedResolve("saved-resolve");
          return;
        case "promise-never":
          return new Promise(r => {});

        case "callback-save":
          savedRespond = respond;
          return true;
        case "callback-call":
          savedRespond("saved-respond");
          return;
        case "callback-never":
          return true;
      }
    });

    const frame = document.createElement("iframe");
    frame.src = "page.html";
    document.body.appendChild(frame);
  }

  function page() {
    browser.test.onMessage.addListener(msg => {
      browser.runtime.sendMessage(msg)
        .then(
          response => {
            if (response) {
              browser.test.log(`Got response: ${response}`);
              browser.test.sendMessage(response);
            }
          }, ({message}) => {
            browser.test.assertTrue(
              /at background@moz-extension:\/\/[\w-]+\/%7B[\w-]+%7D\.js:4:\d went out/.test(message),
              `Promise rejected with the correct error message: ${message}`);
            browser.test.sendMessage("rejected");
          });
    });
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    files: {
      "page.html": "<!DOCTYPE html><meta charset=utf-8><script src=page.js></script>",
      "page.js": page,
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  // Setup long-running tasks before GC.
  extension.sendMessage("promise-save");
  extension.sendMessage("callback-save");

  // Test returning a Promise that can never resolve.
  extension.sendMessage("promise-never");

  extension.sendMessage("ping");
  await extension.awaitMessage("pong");

  Services.ppmm.loadProcessScript("data:,Components.utils.forceGC()", false);
  await extension.awaitMessage("rejected");

  // Test returning `true` without holding the response handle.
  extension.sendMessage("callback-never");

  extension.sendMessage("ping");
  await extension.awaitMessage("pong");

  Services.ppmm.loadProcessScript("data:,Components.utils.forceGC()", false);
  await extension.awaitMessage("rejected");

  // Test that promises from long-running tasks didn't get GCd.
  extension.sendMessage("promise-resolve");
  await extension.awaitMessage("saved-resolve");

  extension.sendMessage("callback-call");
  await extension.awaitMessage("saved-respond");

  ok("Long running tasks responded");
  await extension.unload();
});
