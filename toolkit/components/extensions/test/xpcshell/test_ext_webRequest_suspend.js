"use strict";

const HOSTS = new Set(["example.com"]);

const server = createHttpServer({ hosts: HOSTS });

const BASE_URL = "http://example.com";
const FETCH_ORIGIN = "http://example.com/dummy";

server.registerPathHandler("/return_headers.sjs", (request, response) => {
  response.setHeader("Content-Type", "text/plain", false);

  let headers = {};
  for (let { data: header } of request.headers) {
    headers[header.toLowerCase()] = request.getHeader(header);
  }

  response.write(JSON.stringify(headers));
});

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});

add_task(async function test_suspend() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", `${BASE_URL}/`],
    },

    background() {
      browser.webRequest.onBeforeSendHeaders.addListener(
        details => {
          // Make sure that returning undefined or a promise that resolves to
          // undefined does not break later handlers.
        },
        { urls: ["<all_urls>"] },
        ["blocking", "requestHeaders"]
      );

      browser.webRequest.onBeforeSendHeaders.addListener(
        details => {
          return Promise.resolve();
        },
        { urls: ["<all_urls>"] },
        ["blocking", "requestHeaders"]
      );

      browser.webRequest.onBeforeSendHeaders.addListener(
        details => {
          let requestHeaders = details.requestHeaders.concat({
            name: "Foo",
            value: "Bar",
          });

          return new Promise(resolve => {
            // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
            setTimeout(resolve, 500);
          }).then(() => {
            return { requestHeaders };
          });
        },
        { urls: ["<all_urls>"] },
        ["blocking", "requestHeaders"]
      );
    },
  });

  await extension.startup();

  let headers = JSON.parse(
    await ExtensionTestUtils.fetch(
      FETCH_ORIGIN,
      `${BASE_URL}/return_headers.sjs`
    )
  );

  equal(
    headers.foo,
    "Bar",
    "Request header was correctly set on suspended request"
  );

  await extension.unload();
});

// Test that requests that were canceled while suspended for a blocking
// listener are correctly resumed.
add_task(async function test_error_resume() {
  let observer = channel => {
    if (
      channel instanceof Ci.nsIHttpChannel &&
      channel.URI.spec === "http://example.com/dummy"
    ) {
      Services.obs.removeObserver(observer, "http-on-before-connect");

      // Wait until the next tick to make sure this runs after WebRequest observers.
      Promise.resolve().then(() => {
        channel.cancel(Cr.NS_BINDING_ABORTED);
      });
    }
  };

  Services.obs.addObserver(observer, "http-on-before-connect");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", `${BASE_URL}/`],
    },

    background() {
      browser.webRequest.onBeforeSendHeaders.addListener(
        details => {
          browser.test.log(`onBeforeSendHeaders({url: ${details.url}})`);

          if (details.url === "http://example.com/dummy") {
            browser.test.sendMessage("got-before-send-headers");
          }
        },
        { urls: ["<all_urls>"] },
        ["blocking"]
      );

      browser.webRequest.onErrorOccurred.addListener(
        details => {
          browser.test.log(`onErrorOccurred({url: ${details.url}})`);

          if (details.url === "http://example.com/dummy") {
            browser.test.sendMessage("got-error-occurred");
          }
        },
        { urls: ["<all_urls>"] }
      );
    },
  });

  await extension.startup();

  try {
    await ExtensionTestUtils.fetch(FETCH_ORIGIN, `${BASE_URL}/dummy`);
    ok(false, "Fetch should have failed.");
  } catch (e) {
    ok(true, "Got expected error.");
  }

  await extension.awaitMessage("got-before-send-headers");
  await extension.awaitMessage("got-error-occurred");

  // Wait for the next tick so the onErrorRecurred response can be
  // processed before shutting down the extension.
  await new Promise(resolve => executeSoon(resolve));

  await extension.unload();
});

// Test that response header modifications take effect before onStartRequest fires.
add_task(async function test_set_responseHeaders() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },

    background() {
      browser.webRequest.onHeadersReceived.addListener(
        details => {
          browser.test.log(`onHeadersReceived({url: ${details.url}})`);

          details.responseHeaders.push({ name: "foo", value: "bar" });

          return { responseHeaders: details.responseHeaders };
        },
        { urls: ["http://example.com/?modify_headers"] },
        ["blocking", "responseHeaders"]
      );
    },
  });

  await extension.startup();

  await new Promise(resolve => setTimeout(resolve, 0));

  let resolveHeaderPromise;
  let headerPromise = new Promise(resolve => {
    resolveHeaderPromise = resolve;
  });
  {
    const { Services } = ChromeUtils.import(
      "resource://gre/modules/Services.jsm"
    );

    let ssm = Services.scriptSecurityManager;

    let channel = NetUtil.newChannel({
      uri: "http://example.com/?modify_headers",
      loadingPrincipal: ssm.createContentPrincipalFromOrigin(
        "http://example.com"
      ),
      contentPolicyType: Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
    });

    channel.asyncOpen({
      QueryInterface: ChromeUtils.generateQI([Ci.nsIStreamListener]),

      onStartRequest(request) {
        request.QueryInterface(Ci.nsIHttpChannel);

        try {
          resolveHeaderPromise(request.getResponseHeader("foo"));
        } catch (e) {
          resolveHeaderPromise(null);
        }
        request.cancel(Cr.NS_BINDING_ABORTED);
      },

      onStopRequest() {},

      onDataAvailable() {
        throw new Components.Exception("", Cr.NS_ERROR_FAILURE);
      },
    });
  }

  let headerValue = await headerPromise;
  equal(headerValue, "bar", "Expected Foo header value");

  await extension.unload();
});

// Test that exceptions raised from a blocking webRequest listener that returns
// a promise are logged as expected.
add_task(async function test_logged_error_on_promise_result() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", `${BASE_URL}/`],
    },

    background() {
      async function onBeforeRequest() {
        throw new Error("Expected webRequest exception from a promise result");
      }

      let exceptionRaised = false;

      browser.webRequest.onBeforeRequest.addListener(
        () => {
          if (exceptionRaised) {
            return;
          }

          // We only need to raise the exception once.
          exceptionRaised = true;
          return onBeforeRequest();
        },
        {
          urls: ["http://example.com/*"],
          types: ["main_frame"],
        },
        ["blocking"]
      );

      browser.webRequest.onBeforeRequest.addListener(
        () => {
          browser.test.sendMessage("web-request-event-received");
        },
        {
          urls: ["http://example.com/*"],
          types: ["main_frame"],
        },
        ["blocking"]
      );
    },
  });

  let { messages } = await promiseConsoleOutput(async () => {
    await extension.startup();

    let contentPage = await ExtensionTestUtils.loadContentPage(
      `${BASE_URL}/dummy`
    );
    await extension.awaitMessage("web-request-event-received");
    await contentPage.close();
  });

  ok(
    messages.some(msg =>
      /Expected webRequest exception from a promise result/.test(msg.message)
    ),
    "Got expected console message"
  );

  await extension.unload();
});
