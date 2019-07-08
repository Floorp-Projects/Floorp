"use strict";

const PREF_DISABLE_SECURITY =
  "security.turn_off_all_security_so_that_" +
  "viruses_can_take_over_this_computer";

const HOSTS = new Set(["example.com", "example.org"]);

const server = createHttpServer({ hosts: HOSTS });

const BASE_URL = "http://example.com";

server.registerDirectory("/data/", do_get_file("data"));

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

add_task(async function test_permissions() {
  function background() {
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        if (details.url.includes("_original")) {
          let redirectUrl = details.url
            .replace("example.org", "example.com")
            .replace("_original", "_redirected");
          return { redirectUrl };
        }
        return {};
      },
      { urls: ["<all_urls>"] },
      ["blocking"]
    );
  }

  let extensionData = {
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "<all_urls>"],
    },
    background,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  const frameScript = () => {
    const messageListener = {
      async receiveMessage({ target, messageName, recipient, data, name }) {
        /* globals content */
        let doc = content.document;
        let iframe = doc.createElement("iframe");
        doc.body.appendChild(iframe);

        let promise = new Promise(resolve => {
          let listener = event => {
            content.removeEventListener("message", listener);
            resolve(event.data);
          };
          content.addEventListener("message", listener);
        });

        iframe.setAttribute(
          "src",
          "http://example.com/data/file_WebRequest_permission_original.html"
        );
        let result = await promise;
        doc.body.removeChild(iframe);
        return result;
      },
    };

    const { MessageChannel } = ChromeUtils.import(
      "resource://gre/modules/MessageChannel.jsm"
    );
    MessageChannel.addListener(this, "Test:Check", messageListener);
  };

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/dummy`
  );
  await contentPage.loadFrameScript(frameScript);

  let results = await contentPage.sendMessage("Test:Check", {});
  equal(
    results.page,
    "redirected",
    "Regular webRequest redirect works on an unprivileged page"
  );
  equal(
    results.script,
    "redirected",
    "Regular webRequest redirect works from an unprivileged page"
  );

  Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, true);
  Services.prefs.setBoolPref("extensions.webapi.testing", true);
  Services.prefs.setBoolPref("extensions.webapi.testing.http", true);

  results = await contentPage.sendMessage("Test:Check", {});
  equal(
    results.page,
    "original",
    "webRequest redirect fails on a privileged page"
  );
  equal(
    results.script,
    "original",
    "webRequest redirect fails from a privileged page"
  );

  await extension.unload();
  await contentPage.close();
});

add_task(async function test_no_webRequestBlocking_error() {
  function background() {
    const expectedError =
      "Using webRequest.addListener with the blocking option " +
      "requires the 'webRequestBlocking' permission.";

    const blockingEvents = [
      "onBeforeRequest",
      "onBeforeSendHeaders",
      "onHeadersReceived",
      "onAuthRequired",
    ];

    for (let eventName of blockingEvents) {
      browser.test.assertThrows(
        () => {
          browser.webRequest[eventName].addListener(
            details => {},
            { urls: ["<all_urls>"] },
            ["blocking"]
          );
        },
        expectedError,
        `Got the expected exception for a blocking webRequest.${eventName} listener`
      );
    }
  }

  const extensionData = {
    manifest: { permissions: ["webRequest", "<all_urls>"] },
    background,
  };

  const extension = ExtensionTestUtils.loadExtension(extensionData);

  await extension.startup();
  await extension.unload();
});
