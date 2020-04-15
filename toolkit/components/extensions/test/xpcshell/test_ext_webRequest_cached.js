"use strict";

const BASE_URL = "http://example.com";
const FETCH_ORIGIN = "http://example.com/data/file_sample.html";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));
server.registerPathHandler("/status", (request, response) => {
  let IfNoneMatch = request.hasHeader("If-None-Match")
    ? request.getHeader("If-None-Match")
    : "";

  switch (IfNoneMatch) {
    case "1234567890":
      response.setStatusLine("1.1", 304, "Not Modified");
      response.setHeader("Content-Type", "text/html", false);
      response.setHeader("Etag", "1234567890", false);
      break;
    case "":
      response.setStatusLine("1.1", 200, "OK");
      response.setHeader("Content-Type", "text/html", false);
      response.setHeader("Etag", "1234567890", false);
      response.write("ok");
      break;
    default:
      throw new Error(`Unexpected If-None-Match: ${IfNoneMatch}`);
  }
});

// This test initialises a cache entry with a CSP header, then
// loads the cached entry and replaces the CSP header with
// a new one.  We test in onResponseStarted that the header
// is what we expect.
add_task(async function test_replaceResponseHeaders() {
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      function replaceHeader(headers, newHeader) {
        headers = headers.filter(header => header.name !== newHeader.name);
        headers.push(newHeader);
        return headers;
      }
      let testHeaders = [
        {
          name: "Content-Security-Policy",
          value: "object-src 'none'; script-src 'none'",
        },
        {
          name: "Content-Security-Policy",
          value: "object-src 'none'; script-src https:",
        },
      ];
      let combinedHeaders = {
        name: "Content-Security-Policy",
        value: `${testHeaders[0].value}, ${testHeaders[1].value}`,
      };
      browser.webRequest.onHeadersReceived.addListener(
        details => {
          if (!details.fromCache) {
            // Add a CSP header on the initial request
            details.responseHeaders.push(testHeaders[0]);
            return {
              responseHeaders: details.responseHeaders,
            };
          }
          // Test that the header added during the initial request is
          // now in the cached response.
          let header = details.responseHeaders.filter(header => {
            browser.test.log(`header ${header.name} = ${header.value}`);
            return header.name == "Content-Security-Policy";
          });
          browser.test.assertEq(
            header[0].value,
            testHeaders[0].value,
            "pre-cached header exists"
          );
          // Replace the cached value so we can test overriding the header that was cached.
          return {
            responseHeaders: replaceHeader(
              details.responseHeaders,
              testHeaders[1]
            ),
          };
        },
        {
          urls: ["http://example.com/*/file_sample.html?r=*"],
        },
        ["blocking", "responseHeaders"]
      );
      browser.webRequest.onResponseStarted.addListener(
        details => {
          let needle = details.fromCache ? combinedHeaders : testHeaders[0];
          let header = details.responseHeaders.filter(header => {
            browser.test.log(`header ${header.name} = ${header.value}`);
            return header.name == needle.name && header.value == needle.value;
          });
          browser.test.assertEq(
            header.length,
            1,
            "header exists with correct value"
          );
          if (details.fromCache) {
            browser.test.sendMessage("from-cache");
          }
        },
        {
          urls: ["http://example.com/*/file_sample.html?r=*"],
        },
        ["responseHeaders"]
      );
    },

    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },
  });

  await extension.startup();

  let url = `${BASE_URL}/data/file_sample.html?r=${Math.random()}`;
  await ExtensionTestUtils.fetch(FETCH_ORIGIN, url);
  await ExtensionTestUtils.fetch(FETCH_ORIGIN, url);
  await extension.awaitMessage("from-cache");

  await extension.unload();
});

// This test initialises a cache entry with a CSP header, then
// loads the cached entry and adds a second CSP header.  We also
// test that the browser has the CSP entries we expect.
add_task(async function test_addCSPHeaders() {
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let testHeaders = [
        {
          name: "Content-Security-Policy",
          value: "object-src 'none'; script-src 'none'",
        },
        {
          name: "Content-Security-Policy",
          value: "object-src 'none'; script-src https:",
        },
      ];
      browser.webRequest.onHeadersReceived.addListener(
        details => {
          if (!details.fromCache) {
            details.responseHeaders.push(testHeaders[0]);
            return {
              responseHeaders: details.responseHeaders,
            };
          }
          browser.test.log("cached request received");
          details.responseHeaders.push(testHeaders[1]);
          return {
            responseHeaders: details.responseHeaders,
          };
        },
        {
          urls: ["http://example.com/*/file_sample.html?r=*"],
        },
        ["blocking", "responseHeaders"]
      );
      browser.webRequest.onCompleted.addListener(
        details => {
          let { name, value } = testHeaders[0];
          if (details.fromCache) {
            value = `${value}, ${testHeaders[1].value}`;
          }
          let header = details.responseHeaders.filter(header => {
            browser.test.log(`header ${header.name} = ${header.value}`);
            return header.name == name && header.value == value;
          });
          browser.test.assertEq(
            header.length,
            1,
            "header exists with correct value"
          );
          if (details.fromCache) {
            browser.test.sendMessage("from-cache");
          }
        },
        {
          urls: ["http://example.com/*/file_sample.html?r=*"],
        },
        ["responseHeaders"]
      );
    },

    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },
  });

  await extension.startup();

  let url = `${BASE_URL}/data/file_sample.html?r=${Math.random()}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url);
  equal(contentPage.browser.csp.policyCount, 1, "expected 1 policy");
  equal(
    contentPage.browser.csp.getPolicy(0),
    "object-src 'none'; script-src 'none'",
    "expected policy"
  );
  await contentPage.close();

  contentPage = await ExtensionTestUtils.loadContentPage(url);
  equal(contentPage.browser.csp.policyCount, 2, "expected 2 policies");
  equal(
    contentPage.browser.csp.getPolicy(0),
    "object-src 'none'; script-src 'none'",
    "expected first policy"
  );
  equal(
    contentPage.browser.csp.getPolicy(1),
    "object-src 'none'; script-src https:",
    "expected second policy"
  );

  await extension.awaitMessage("from-cache");
  await contentPage.close();

  await extension.unload();
});

// This test verifies that a content type changed during
// onHeadersReceived is cached.  We initialize the cache,
// then load against a url that will specifically return
// a 304 status code.
add_task(async function test_addContentTypeHeaders() {
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.webRequest.onBeforeSendHeaders.addListener(
        details => {
          browser.test.log(`onBeforeSendHeaders ${JSON.stringify(details)}\n`);
        },
        {
          urls: ["http://example.com/status*"],
        },
        ["blocking", "requestHeaders"]
      );
      browser.webRequest.onHeadersReceived.addListener(
        details => {
          browser.test.log(`onHeadersReceived ${JSON.stringify(details)}\n`);
          if (!details.fromCache) {
            browser.test.sendMessage("statusCode", details.statusCode);
            const mime = details.responseHeaders.find(header => {
              return header.value && header.name === "content-type";
            });
            if (mime) {
              mime.value = "text/plain";
            } else {
              details.responseHeaders.push({
                name: "content-type",
                value: "text/plain",
              });
            }
            return {
              responseHeaders: details.responseHeaders,
            };
          }
        },
        {
          urls: ["http://example.com/status*"],
        },
        ["blocking", "responseHeaders"]
      );
      browser.webRequest.onCompleted.addListener(
        details => {
          browser.test.log(`onCompleted ${JSON.stringify(details)}\n`);
          const mime = details.responseHeaders.find(header => {
            return header.value && header.name === "content-type";
          });
          browser.test.sendMessage("contentType", mime.value);
        },
        {
          urls: ["http://example.com/status*"],
        },
        ["responseHeaders"]
      );
    },

    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/status`
  );
  equal(await extension.awaitMessage("statusCode"), "200", "status OK");
  equal(
    await extension.awaitMessage("contentType"),
    "text/plain",
    "plain text header"
  );
  await contentPage.close();

  contentPage = await ExtensionTestUtils.loadContentPage(`${BASE_URL}/status`);
  equal(await extension.awaitMessage("statusCode"), "304", "not modified");
  equal(
    await extension.awaitMessage("contentType"),
    "text/plain",
    "plain text header"
  );
  await contentPage.close();

  await extension.unload();
});
