/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const server = createHttpServer({
  // We need the 127.0.0.1 proxy because the sec-fetch headers are not sent to
  // "127.0.0.1:<any port other than 80 or 443>".
  hosts: ["127.0.0.1", "127.0.0.2"],
});

server.registerPathHandler("/page.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Access-Control-Allow-Origin", "*");
});

server.registerPathHandler("/return_headers", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Access-Control-Allow-Origin", "*");
  let headers = {};
  for (let header of [
    "sec-fetch-site",
    "sec-fetch-dest",
    "sec-fetch-mode",
    "sec-fetch-user",
  ]) {
    if (request.hasHeader(header)) {
      headers[header] = request.getHeader(header);
    }
  }
  response.write(JSON.stringify(headers));
});

async function contentScript() {
  const results = await Promise.all([
    // A cross-origin request from the content script.
    // Sending requests with CORS from content scripts is currently not possible
    // (Bug 1605197)
    //fetch("http://127.0.0.1/return_headers").then(res => res.json()),

    // A cross-origin request that behaves as if it was sent by the content it
    // self.
    content.fetch("http://127.0.0.1/return_headers").then(res => res.json()),
    // A same-origin request that behaves as if it was sent by the content it
    // self.
    content.fetch("http://127.0.0.2/return_headers").then(res => res.json()),
    // A same-origin request from the content script.
    fetch("http://127.0.0.2/return_headers").then(res => res.json()),
  ]);

  browser.test.sendMessage("content_results", results);
}

async function runSecFetchTest(test) {
  let data = {
    async background() {
      let site = await new Promise(resolve => {
        browser.test.onMessage.addListener(msg => {
          resolve(msg);
        });
      });

      let headers = await (await fetch(`${site}/return_headers`)).json();
      browser.test.sendMessage("background_results", headers);
    },
    manifest: {
      manifest_version: 2,
      content_scripts: [
        {
          matches: ["http://127.0.0.2/*"],
          js: ["content_script.js"],
        },
      ],
    },
    files: {
      "content_script.js": contentScript,
    },
  };

  // The sec-fetch-* headers are only send to potentially trust worthy origins.
  // We use 127.0.0.1 to avoid setting up an https server.
  const site = "http://127.0.0.1";

  if (test.permission) {
    data.manifest.permissions = ["http://127.0.0.2/*", `${site}/*`];
  }

  let extension = ExtensionTestUtils.loadExtension(data);
  await extension.startup();

  extension.sendMessage(site);
  let backgroundResults = await extension.awaitMessage("background_results");
  Assert.deepEqual(backgroundResults, test.expectedBackgroundHeaders);

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `http://127.0.0.2/page.html`
  );
  let contentResults = await extension.awaitMessage("content_results");
  Assert.deepEqual(contentResults, test.expectedContentHeaders);
  await contentPage.close();

  await extension.unload();
}

add_task(async function test_fetch_without_permissions() {
  await runSecFetchTest({
    permission: false,
    expectedBackgroundHeaders: {
      "sec-fetch-site": "cross-site",
      "sec-fetch-mode": "cors",
      "sec-fetch-dest": "empty",
    },
    expectedContentHeaders: [
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
      },
      {
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
      },
      {
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
      },
    ],
  });
});

add_task(async function test_fetch_with_permissions() {
  await runSecFetchTest({
    permission: true,
    expectedBackgroundHeaders: {
      "sec-fetch-site": "same-origin",
      "sec-fetch-mode": "cors",
      "sec-fetch-dest": "empty",
    },
    expectedContentHeaders: [
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
      },
      {
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
      },
      {
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
      },
    ],
  });
});
