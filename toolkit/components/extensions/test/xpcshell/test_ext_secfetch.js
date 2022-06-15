/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

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
  if (request.method === "OPTIONS") {
    // Handle CORS preflight request.
    response.setHeader("Access-Control-Allow-Methods", "GET, PUT");
    return;
  }

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

  if (request.hasHeader("origin")) {
    headers.origin = request
      .getHeader("origin")
      .replace(/moz-extension:\/\/[^\/]+/, "moz-extension://<placeholder>");
  }

  response.write(JSON.stringify(headers));
});

async function contentScript() {
  let content_fetch;
  if (browser.runtime.getManifest().manifest_version === 2) {
    content_fetch = content.fetch;
  } else {
    // In MV3, there is no content variable.
    browser.test.assertEq(typeof content, "undefined", "no .content in MV3");
    // In MV3, window.fetch is the original fetch with the page's principal.
    content_fetch = window.fetch.bind(window);
  }
  let results = await Promise.allSettled([
    // A cross-origin request from the content script.
    fetch("http://127.0.0.1/return_headers").then(res => res.json()),
    // A cross-origin request that behaves as if it was sent by the content it
    // self.
    content_fetch("http://127.0.0.1/return_headers").then(res => res.json()),
    // A same-origin request that behaves as if it was sent by the content it
    // self.
    content_fetch("http://127.0.0.2/return_headers").then(res => res.json()),
    // A same-origin request from the content script.
    fetch("http://127.0.0.2/return_headers").then(res => res.json()),
    // Non GET or HEAD request, triggers CORS preflight.
    fetch("http://127.0.0.2/return_headers", { method: "PUT" }).then(res =>
      res.json()
    ),
  ]);

  results = results.map(({ value, reason }) => value ?? reason.message);

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

      let results = await Promise.all([
        fetch(`${site}/return_headers`).then(res => res.json()),
        // Non GET or HEAD request, triggers CORS preflight.
        fetch(`${site}/return_headers`, { method: "PUT" }).then(res =>
          res.json()
        ),
      ]);
      browser.test.sendMessage("background_results", results);
    },
    manifest: {
      manifest_version: test.manifest_version,
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

  if (data.manifest.manifest_version == 3) {
    // Automatically grant permissions so that the content script can run.
    data.manifest.granted_host_permissions = true;
    // Needed to use granted_host_permissions in tests:
    data.temporarilyInstalled = true;
    // Work-around for bug 1766752:
    data.manifest.host_permissions = ["http://127.0.0.2/*"];
    // (note: ^ host_permissions may be replaced/extended below).
  }

  // The sec-fetch-* headers are only send to potentially trust worthy origins.
  // We use 127.0.0.1 to avoid setting up an https server.
  const site = "http://127.0.0.1";

  if (test.permission) {
    // MV3 requires permissions to be set in permissions. ExtensionTestCommon
    // will replace host_permissions with permissions in MV2.
    data.manifest.host_permissions = ["http://127.0.0.2/*", `${site}/*`];
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

add_task(async function test_fetch_without_permissions_mv2() {
  await runSecFetchTest({
    manifest_version: 2,
    permission: false,
    expectedBackgroundHeaders: [
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "moz-extension://<placeholder>",
      },
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "moz-extension://<placeholder>",
      },
    ],
    expectedContentHeaders: [
      // TODO bug 1605197: Support cors without permissions in MV2.
      "NetworkError when attempting to fetch resource.",
      // Expectation:
      // {
      //   "sec-fetch-site": "cross-site",
      //   "sec-fetch-mode": "cors",
      //   "sec-fetch-dest": "empty",
      // },
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "http://127.0.0.2",
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
      {
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "null",
      },
    ],
  });
});

add_task(async function test_fetch_with_permissions_mv2() {
  await runSecFetchTest({
    manifest_version: 2,
    permission: true,
    expectedBackgroundHeaders: [
      {
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
      },
      {
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "moz-extension://<placeholder>",
      },
    ],
    expectedContentHeaders: [
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
      },
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "http://127.0.0.2",
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
      {
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "null",
      },
    ],
  });
});

add_task(async function test_fetch_without_permissions_mv3() {
  await runSecFetchTest({
    manifest_version: 3,
    permission: false,
    expectedBackgroundHeaders: [
      // Same as in test_fetch_without_permissions_mv2.
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "moz-extension://<placeholder>",
      },
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "moz-extension://<placeholder>",
      },
    ],
    expectedContentHeaders: [
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "http://127.0.0.2",
      },
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "http://127.0.0.2",
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
      {
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "http://127.0.0.2",
      },
    ],
  });
});

add_task(async function test_fetch_with_permissions_mv3() {
  await runSecFetchTest({
    manifest_version: 3,
    permission: true,
    expectedBackgroundHeaders: [
      {
        // Same as in test_fetch_with_permissions_mv2.
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
      },
      {
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "moz-extension://<placeholder>",
      },
    ],
    expectedContentHeaders: [
      // All expectations the same as in test_fetch_without_permissions_mv3.
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "http://127.0.0.2",
      },
      {
        "sec-fetch-site": "cross-site",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "http://127.0.0.2",
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
      {
        "sec-fetch-site": "same-origin",
        "sec-fetch-mode": "cors",
        "sec-fetch-dest": "empty",
        origin: "http://127.0.0.2",
      },
    ],
  });
});
