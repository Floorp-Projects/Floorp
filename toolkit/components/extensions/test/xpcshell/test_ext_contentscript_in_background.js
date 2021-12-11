/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/dummyFrame", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.write("");
});

add_task(async function connect_from_background_frame() {
  async function background() {
    const FRAME_URL = "http://example.com:8888/dummyFrame";
    browser.runtime.onConnect.addListener(port => {
      browser.test.assertEq(port.sender.tab, undefined, "Sender is not a tab");
      browser.test.assertEq(port.sender.url, FRAME_URL, "Expected sender URL");
      port.onMessage.addListener(msg => {
        browser.test.assertEq("pong", msg, "Reply from content script");
        port.disconnect();
      });
      port.postMessage("ping");
    });

    await browser.contentScripts.register({
      matches: ["http://example.com/dummyFrame"],
      js: [{ file: "contentscript.js" }],
      allFrames: true,
    });

    let f = document.createElement("iframe");
    f.src = FRAME_URL;
    document.body.appendChild(f);
  }

  function contentScript() {
    browser.test.log(`Running content script at ${document.URL}`);

    let port = browser.runtime.connect();
    port.onMessage.addListener(msg => {
      browser.test.assertEq("ping", msg, "Expected message to content script");
      port.postMessage("pong");
    });
    port.onDisconnect.addListener(() => {
      browser.test.sendMessage("disconnected_in_content_script");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://example.com/*"],
    },
    files: {
      "contentscript.js": contentScript,
    },
    background,
  });
  await extension.startup();
  await extension.awaitMessage("disconnected_in_content_script");
  await extension.unload();
});
