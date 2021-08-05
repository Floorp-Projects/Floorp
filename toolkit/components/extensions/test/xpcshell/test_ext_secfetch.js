/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const server = createHttpServer({
  // We need the 127.0.0.1 proxy because the sec-fetch headers are not sent to
  // "127.0.0.1:<any port other than 80 or 443>".
  hosts: ["127.0.0.1"],
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
    },
  };

  // The sec-fetch-* headers are only send to potentially trust worthy origins.
  // We use 127.0.0.1 to avoid setting up an https server.
  const site = "http://127.0.0.1";

  if (test.permission) {
    data.manifest.permissions = [`${site}/*`];
  }

  let extension = ExtensionTestUtils.loadExtension(data);
  await extension.startup();

  extension.sendMessage(site);
  let backgroundResults = await extension.awaitMessage("background_results");
  Assert.deepEqual(backgroundResults, test.expectedHeaders);

  await extension.unload();
}

add_task(async function test_background_fetch_without_permission() {
  await runSecFetchTest({
    permission: false,
    expectedHeaders: {
      "sec-fetch-site": "cross-site",
      "sec-fetch-mode": "cors",
      "sec-fetch-dest": "empty",
    },
  });
});

add_task(async function test_background_fetch_with_permission() {
  await runSecFetchTest({
    permission: true,
    expectedHeaders: {
      "sec-fetch-site": "same-origin",
      "sec-fetch-mode": "cors",
      "sec-fetch-dest": "empty",
    },
  });
});
