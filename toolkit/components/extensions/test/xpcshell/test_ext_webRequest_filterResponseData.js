"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const HOSTS = new Set(["example.com", "example.org", "example.net"]);

const server = createHttpServer({ hosts: HOSTS });

const FETCH_ORIGIN = "http://example.com/dummy";

server.registerDirectory("/data/", do_get_file("data"));

server.registerPathHandler("/redirect", (request, response) => {
  let params = new URLSearchParams(request.queryString);
  response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
  response.setHeader("Location", params.get("redirect_uri"));
  response.setHeader("Access-Control-Allow-Origin", "*");
});

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.write("ok");
});

server.registerPathHandler("/dummy.xhtml", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/xhtml+xml");
  response.write(String.raw`<?xml version="1.0"?>
    <html xml:lang="en" xmlns="http://www.w3.org/1999/xhtml">
      <head/>
      <body/>
    </html>
  `);
});

server.registerPathHandler("/lorem.html.gz", async (request, response) => {
  response.processAsync();

  response.setHeader(
    "Content-Type",
    "Content-Type: text/html; charset=utf-8",
    false
  );
  response.setHeader("Content-Encoding", "gzip", false);

  let data = await OS.File.read(do_get_file("data/lorem.html.gz").path);
  response.write(String.fromCharCode(...new Uint8Array(data)));

  response.finish();
});

// Test re-encoding the data stream for bug 1590898.
add_task(async function test_stream_encoding_data() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        request => {
          let filter = browser.webRequest.filterResponseData(request.requestId);
          let decoder = new TextDecoder("utf-8");
          let encoder = new TextEncoder();

          filter.ondata = event => {
            let str = decoder.decode(event.data, { stream: true });
            filter.write(encoder.encode(str));
            filter.disconnect();
          };
        },
        {
          urls: ["http://example.com/lorem.html.gz"],
          types: ["main_frame"],
        },
        ["blocking"]
      );
    },

    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/lorem.html.gz"
  );

  let content = await contentPage.spawn(null, () => {
    return this.content.document.body.textContent;
  });

  ok(
    content.includes("Lorem ipsum dolor sit amet"),
    `expected content received`
  );

  await contentPage.close();
  await extension.unload();
});

// Tests that the stream filter request is added to the document's load
// group, and blocks an XML document's load event until after the filter
// stops sending data.
add_task(async function test_xml_document_loadgroup_blocking() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        request => {
          let filter = browser.webRequest.filterResponseData(request.requestId);

          let data = [];
          filter.ondata = event => {
            data.push(event.data);
          };
          filter.onstop = async () => {
            browser.test.sendMessage("phase", "original-onstop");

            // Make a few trips through the event loop.
            for (let i = 0; i < 10; i++) {
              await new Promise(resolve => setTimeout(resolve, 0));
            }

            for (let buffer of data) {
              filter.write(buffer);
            }
            browser.test.sendMessage("phase", "filter-onstop");
            filter.close();
          };
        },
        {
          urls: ["http://example.com/dummy.xhtml"],
        },
        ["blocking"]
      );
    },

    files: {
      "content_script.js"() {
        browser.test.sendMessage("phase", "content-script-start");
        window.addEventListener(
          "DOMContentLoaded",
          () => {
            browser.test.sendMessage("phase", "content-script-domload");
          },
          { once: true }
        );
        window.addEventListener(
          "load",
          () => {
            browser.test.sendMessage("phase", "content-script-load");
          },
          { once: true }
        );
      },
    },

    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],

      content_scripts: [
        {
          matches: ["http://example.com/dummy.xhtml"],
          run_at: "document_start",
          js: ["content_script.js"],
        },
      ],
    },
  });

  await extension.startup();

  const EXPECTED = [
    "original-onstop",
    "filter-onstop",
    "content-script-start",
    "content-script-domload",
    "content-script-load",
  ];

  let done = new Promise(resolve => {
    let phases = [];
    extension.onMessage("phase", phase => {
      phases.push(phase);
      if (phases.length === EXPECTED.length) {
        resolve(phases);
      }
    });
  });

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy.xhtml"
  );

  deepEqual(await done, EXPECTED, "Things happened, and in the right order");

  await contentPage.close();
  await extension.unload();
});

add_task(async function() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let pending = [];

      browser.webRequest.onBeforeRequest.addListener(
        data => {
          let filter = browser.webRequest.filterResponseData(data.requestId);

          let url = new URL(data.url);

          if (url.searchParams.get("redirect_uri")) {
            pending.push(
              new Promise(resolve => {
                filter.onerror = resolve;
              }).then(() => {
                browser.test.assertEq(
                  "Channel redirected",
                  filter.error,
                  "Got correct error for redirected filter"
                );
              })
            );
          }

          filter.onstart = () => {
            filter.write(new TextEncoder().encode(data.url));
          };
          filter.ondata = event => {
            let str = new TextDecoder().decode(event.data);
            browser.test.assertEq(
              "ok",
              str,
              `Got unfiltered data for ${data.url}`
            );
          };
          filter.onstop = () => {
            filter.close();
          };
        },
        {
          urls: ["<all_urls>"],
        },
        ["blocking"]
      );

      browser.test.onMessage.addListener(async msg => {
        if (msg === "done") {
          await Promise.all(pending);
          browser.test.notifyPass("stream-filter");
        }
      });
    },

    manifest: {
      permissions: [
        "webRequest",
        "webRequestBlocking",
        "http://example.com/",
        "http://example.org/",
      ],
    },
  });

  await extension.startup();

  let results = [
    ["http://example.com/dummy", "http://example.com/dummy"],
    ["http://example.org/dummy", "http://example.org/dummy"],
    ["http://example.net/dummy", "ok"],
    [
      "http://example.com/redirect?redirect_uri=http://example.com/dummy",
      "http://example.com/dummy",
    ],
    [
      "http://example.com/redirect?redirect_uri=http://example.org/dummy",
      "http://example.org/dummy",
    ],
    ["http://example.com/redirect?redirect_uri=http://example.net/dummy", "ok"],
    [
      "http://example.net/redirect?redirect_uri=http://example.com/dummy",
      "http://example.com/dummy",
    ],
  ].map(async ([url, expectedResponse]) => {
    let text = await ExtensionTestUtils.fetch(FETCH_ORIGIN, url);
    equal(text, expectedResponse, `Expected response for ${url}`);
  });

  await Promise.all(results);

  extension.sendMessage("done");
  await extension.awaitFinish("stream-filter");
  await extension.unload();
});

add_task(async function test_alternate_cached_data() {
  Services.prefs.setBoolPref("dom.script_loader.bytecode_cache.enabled", true);
  Services.prefs.setIntPref("dom.script_loader.bytecode_cache.strategy", -1);

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          let filter = browser.webRequest.filterResponseData(details.requestId);
          let decoder = new TextDecoder("utf-8");
          let encoder = new TextEncoder();

          filter.ondata = event => {
            let str = decoder.decode(event.data, { stream: true });
            filter.write(encoder.encode(str));
            filter.disconnect();
            browser.test.assertTrue(
              str.startsWith(`"use strict";`),
              "ondata received decoded data"
            );
            browser.test.sendMessage("onBeforeRequest");
          };

          filter.onerror = () => {
            // onBeforeRequest will always beat the cache race, so we should always
            // get valid data in ondata.
            browser.test.fail("error-received", filter.error);
          };
        },
        {
          urls: ["http://example.com/data/file_script_good.js"],
        },
        ["blocking"]
      );
      browser.webRequest.onHeadersReceived.addListener(
        details => {
          let filter = browser.webRequest.filterResponseData(details.requestId);
          let decoder = new TextDecoder("utf-8");
          let encoder = new TextEncoder();

          // Because cache is always a race, intermittently we will succesfully
          // beat the cache, in which case we pass in ondata.  If cache wins,
          // we pass in onerror.
          // Running the test with --verify hits this cache race issue, as well
          // it seems that the cache primarily looses on linux1804.
          let gotone = false;
          filter.ondata = event => {
            browser.test.assertFalse(gotone, "cache lost the race");
            gotone = true;
            let str = decoder.decode(event.data, { stream: true });
            filter.write(encoder.encode(str));
            filter.disconnect();
            browser.test.assertTrue(
              str.startsWith(`"use strict";`),
              "ondata received decoded data"
            );
            browser.test.sendMessage("onHeadersReceived");
          };

          filter.onerror = () => {
            browser.test.assertFalse(gotone, "cache won the race");
            gotone = true;
            browser.test.assertEq(
              filter.error,
              "Channel is delivering cached alt-data"
            );
            browser.test.sendMessage("onHeadersReceived");
          };
        },
        {
          urls: ["http://example.com/data/file_script_bad.js"],
        },
        ["blocking"]
      );
    },

    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/*"],
    },
  });

  // Prime the cache so we have the script byte-cached.
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_script.html"
  );
  await contentPage.close();

  await extension.startup();

  let page_cached = await await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_script.html"
  );
  await Promise.all([
    extension.awaitMessage("onBeforeRequest"),
    extension.awaitMessage("onHeadersReceived"),
  ]);
  await page_cached.close();
  await extension.unload();

  Services.prefs.clearUserPref("dom.script_loader.bytecode_cache.enabled");
  Services.prefs.clearUserPref("dom.script_loader.bytecode_cache.strategy");
});
