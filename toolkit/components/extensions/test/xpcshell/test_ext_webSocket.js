"use strict";

const HOSTS = new Set(["example.com"]);

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
// Since we're not using AOM, and MV3 forces event pages, bypass
// delayed-startup for MV3 test.  These tests do not rely on startup events.
Services.prefs.setBoolPref(
  "extensions.webextensions.background-delayed-startup",
  false
);

const server = createHttpServer({ hosts: HOSTS });

const BASE_URL = `http://example.com`;
const pageURL = `${BASE_URL}/plain.html`;

server.registerPathHandler("/plain.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html");
  response.setHeader("Content-Security-Policy", "upgrade-insecure-requests;");
  response.write("<!DOCTYPE html><html></html>");
});

async function testWebSocketInFrameUpgraded(data = {}) {
  const frame = document.createElement("iframe");
  frame.src = browser.runtime.getURL("frame.html");
  document.documentElement.appendChild(frame);
}

async function test_webSocket(version) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: version,
      permissions: ["webRequest", "webRequestBlocking"],
      host_permissions: ["<all_urls>"],
      content_scripts: [
        {
          matches: ["http://*/plain.html"],
          run_at: "document_idle",
          js: ["content_script.js"],
        },
      ],
    },
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          // the websockets should not be upgraded
          browser.test.assertEq(
            "ws:",
            new URL(details.url).protocol,
            "ws protocol worked"
          );
          browser.test.notifyPass("websocket");
        },
        { urls: ["wss://example.com/*", "ws://example.com/*"] },
        ["blocking"]
      );
    },
    files: {
      "frame.html": `
<html>
    <head>
        <meta charset="utf-8"/>
        <script type="application/javascript" src="frame_script.js"></script>
    </head>
    <body>
    </body>
</html>
	  `,
      "frame_script.js": `new WebSocket("ws://example.com/ws_dummy");`,
      "content_script.js": `
      (${testWebSocketInFrameUpgraded})()
      `,
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(pageURL);
  await extension.awaitFinish("websocket");
  await contentPage.close();
  await extension.unload();
}

add_task(async function test_webSocket_upgrade_iframe() {
  await test_webSocket(2);
  await test_webSocket(3);
});
