/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));
const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function runtimeSendMessageReply() {
  function background() {
    browser.runtime.onMessage.addListener((msg, sender, respond) => {
      if (msg == "respond-now") {
        respond(msg);
      } else if (msg == "respond-soon") {
        setTimeout(() => {
          respond(msg);
        }, 0);
        return true;
      } else if (msg == "respond-promise") {
        return Promise.resolve(msg);
      } else if (msg == "respond-promise-false") {
        return Promise.resolve(false);
      } else if (msg == "respond-false") {
        // return false means that respond() is not expected to be called.
        setTimeout(() => respond("should be ignored"));
        return false;
      } else if (msg == "respond-never") {
        return undefined;
      } else if (msg == "respond-error") {
        return Promise.reject(new Error(msg));
      } else if (msg == "throw-error") {
        throw new Error(msg);
      } else if (msg === "respond-uncloneable") {
        return Promise.resolve(window);
      } else if (msg === "reject-uncloneable") {
        return Promise.reject(window);
      } else if (msg == "reject-undefined") {
        return Promise.reject();
      } else if (msg == "throw-undefined") {
        throw undefined; // eslint-disable-line no-throw-literal
      }
    });

    browser.runtime.onMessage.addListener((msg, sender, respond) => {
      if (msg == "respond-now") {
        respond("hello");
      } else if (msg == "respond-now-2") {
        respond(msg);
      }
    });

    browser.runtime.onMessage.addListener((msg, sender, respond) => {
      if (msg == "respond-now") {
        // If a response from another listener is received first, this
        // exception should be ignored.  Test fails if it is not.

        // All this is of course stupid, but some extensions depend on it.
        msg.blah.this.throws();
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
      new Promise(resolve =>
        browser.runtime.sendMessage("respond-soon", resolve)
      ),
      browser.runtime.sendMessage("respond-promise"),
      browser.runtime.sendMessage("respond-promise-false"),
      browser.runtime.sendMessage("respond-false"),
      browser.runtime.sendMessage("respond-never"),
      new Promise(resolve => {
        browser.runtime.sendMessage("respond-never", response => {
          resolve(response);
        });
      }),

      browser.runtime
        .sendMessage("respond-error")
        .catch(error => Promise.resolve({ error })),
      browser.runtime
        .sendMessage("throw-error")
        .catch(error => Promise.resolve({ error })),

      browser.runtime
        .sendMessage("respond-uncloneable")
        .catch(error => Promise.resolve({ error })),
      browser.runtime
        .sendMessage("reject-uncloneable")
        .catch(error => Promise.resolve({ error })),
      browser.runtime
        .sendMessage("reject-undefined")
        .catch(error => Promise.resolve({ error })),
      browser.runtime
        .sendMessage("throw-undefined")
        .catch(error => Promise.resolve({ error })),
    ])
      .then(
        ([
          respondNow,
          respondNow2,
          respondSoon,
          respondPromise,
          respondPromiseFalse,
          respondFalse,
          respondNever,
          respondNever2,
          respondError,
          throwError,
          respondUncloneable,
          rejectUncloneable,
          rejectUndefined,
          throwUndefined,
        ]) => {
          browser.test.assertEq(
            "respond-now",
            respondNow,
            "Got the expected immediate response"
          );
          browser.test.assertEq(
            "respond-now-2",
            respondNow2,
            "Got the expected immediate response from the second listener"
          );
          browser.test.assertEq(
            "respond-soon",
            respondSoon,
            "Got the expected delayed response"
          );
          browser.test.assertEq(
            "respond-promise",
            respondPromise,
            "Got the expected promise response"
          );
          browser.test.assertEq(
            false,
            respondPromiseFalse,
            "Got the expected false value as a promise result"
          );
          browser.test.assertEq(
            undefined,
            respondFalse,
            "Got the expected no-response when onMessage returns false"
          );
          browser.test.assertEq(
            undefined,
            respondNever,
            "Got the expected no-response resolution"
          );
          browser.test.assertEq(
            undefined,
            respondNever2,
            "Got the expected no-response resolution"
          );

          browser.test.assertEq(
            "respond-error",
            respondError.error.message,
            "Got the expected error response"
          );
          browser.test.assertEq(
            "throw-error",
            throwError.error.message,
            "Got the expected thrown error response"
          );

          browser.test.assertEq(
            "Could not establish connection. Receiving end does not exist.",
            respondUncloneable.error.message,
            "An uncloneable response should be ignored"
          );
          browser.test.assertEq(
            "An unexpected error occurred",
            rejectUncloneable.error.message,
            "Got the expected error for a rejection with an uncloneable value"
          );
          browser.test.assertEq(
            "An unexpected error occurred",
            rejectUndefined.error.message,
            "Got the expected error for a void rejection"
          );
          browser.test.assertEq(
            "An unexpected error occurred",
            throwUndefined.error.message,
            "Got the expected error for a void throw"
          );

          browser.test.notifyPass("sendMessage");
        }
      )
      .catch(e => {
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

add_task(async function runtimeSendMessageBlob() {
  function background() {
    browser.runtime.onMessage.addListener(msg => {
      // eslint-disable-next-line mozilla/use-isInstance -- this function runs in an extension
      browser.test.assertTrue(msg.blob instanceof Blob, "Message is a blob");
      return Promise.resolve(msg);
    });

    let childFrame = document.createElement("iframe");
    childFrame.src = "extensionpage.html";
    document.body.appendChild(childFrame);
  }

  function senderScript() {
    browser.runtime
      .sendMessage({ blob: new Blob(["hello"]) })
      .then(response => {
        browser.test.assertTrue(
          // eslint-disable-next-line mozilla/use-isInstance -- this function runs in an extension
          response.blob instanceof Blob,
          "Response is a blob"
        );
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
      browser.runtime.sendMessage(msg).then(
        response => {
          if (response) {
            browser.test.log(`Got response: ${response}`);
            browser.test.sendMessage(response);
          }
        },
        error => {
          browser.test.assertEq(
            "Promised response from onMessage listener went out of scope",
            error.message,
            `Promise rejected with the correct error message`
          );

          browser.test.assertTrue(
            /^moz-extension:\/\/[\w-]+\/%7B[\w-]+%7D\.js/.test(error.fileName),
            `Promise rejected with the correct error filename: ${error.fileName}`
          );

          browser.test.assertEq(
            4,
            error.lineNumber,
            `Promise rejected with the correct error line number`
          );

          browser.test.assertTrue(
            /moz-extension:\/\/[\w-]+\/%7B[\w-]+%7D\.js:4/.test(error.stack),
            `Promise rejected with the correct error stack: ${error.stack}`
          );
          browser.test.sendMessage("rejected");
        }
      );
    });
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    files: {
      "page.html":
        "<!DOCTYPE html><meta charset=utf-8><script src=page.js></script>",
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

  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    true
  );
  Services.ppmm.loadProcessScript("data:,Components.utils.forceGC()", false);
  await extension.awaitMessage("rejected");

  // Test returning `true` without holding the response handle.
  extension.sendMessage("callback-never");

  extension.sendMessage("ping");
  await extension.awaitMessage("pong");

  Services.ppmm.loadProcessScript("data:,Components.utils.forceGC()", false);
  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    false
  );
  await extension.awaitMessage("rejected");

  // Test that promises from long-running tasks didn't get GCd.
  extension.sendMessage("promise-resolve");
  await extension.awaitMessage("saved-resolve");

  extension.sendMessage("callback-call");
  await extension.awaitMessage("saved-respond");

  ok("Long running tasks responded");
  await extension.unload();
});

add_task(async function sendMessage_async_response_multiple_contexts() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.runtime.onMessage.addListener((msg, _, respond) => {
        browser.test.log(`Background got request: ${msg}`);

        switch (msg) {
          case "ask-bg-fast":
            respond("bg-respond");
            return true;

          case "ask-bg-slow":
            return new Promise(r => setTimeout(() => r("bg-promise")), 1000);
        }
      });
      browser.test.sendMessage("bg-ready");
    },

    manifest: {
      content_scripts: [
        {
          matches: ["http://localhost/*/file_sample.html"],
          js: ["cs.js"],
        },
      ],
    },

    files: {
      "page.html":
        "<!DOCTYPE html><meta charset=utf-8><script src=page.js></script>",
      "page.js"() {
        browser.runtime.onMessage.addListener((msg, _, respond) => {
          browser.test.log(`Page got request: ${msg}`);

          switch (msg) {
            case "ask-page-fast":
              respond("page-respond");
              return true;

            case "ask-page-slow":
              return new Promise(r => setTimeout(() => r("page-promise")), 500);
          }
        });
        browser.test.sendMessage("page-ready");
      },

      "cs.js"() {
        Promise.all([
          browser.runtime.sendMessage("ask-bg-fast"),
          browser.runtime.sendMessage("ask-bg-slow"),
          browser.runtime.sendMessage("ask-page-fast"),
          browser.runtime.sendMessage("ask-page-slow"),
        ]).then(responses => {
          browser.test.assertEq(
            responses.join(),
            ["bg-respond", "bg-promise", "page-respond", "page-promise"].join(),
            "Got all expected responses from correct contexts"
          );
          browser.test.notifyPass("cs-done");
        });
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("bg-ready");

  let url = `moz-extension://${extension.uuid}/page.html`;
  let page = await ExtensionTestUtils.loadContentPage(url, { extension });
  await extension.awaitMessage("page-ready");

  let content = await ExtensionTestUtils.loadContentPage(
    BASE_URL + "/file_sample.html"
  );
  await extension.awaitFinish("cs-done");
  await content.close();

  await page.close();
  await extension.unload();
});
