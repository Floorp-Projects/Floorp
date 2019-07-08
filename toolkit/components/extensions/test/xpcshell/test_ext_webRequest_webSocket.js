"use strict";

const HOSTS = new Set(["example.com"]);

const server = createHttpServer({ hosts: HOSTS });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});

add_task(async function test_webSocket() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "<all_urls>"],
    },
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.assertEq(
            "ws:",
            new URL(details.url).protocol,
            "ws protocol worked"
          );
          browser.test.notifyPass("websocket");
        },
        { urls: ["ws://example.com/*"] },
        ["blocking"]
      );

      browser.test.onMessage.addListener(msg => {
        let ws = new WebSocket("ws://example.com/dummy");
        ws.onopen = e => {
          ws.send("data");
        };
        ws.onclose = e => {};
        ws.onerror = e => {};
        ws.onmessage = e => {
          ws.close();
        };
      });
      browser.test.sendMessage("ready");
    },
  });
  await extension.startup();
  await extension.awaitMessage("ready");
  extension.sendMessage("go");
  await extension.awaitFinish("websocket");

  // Wait until the next tick so that listener responses are processed
  // before we unload.
  await new Promise(executeSoon);

  await extension.unload();
});
