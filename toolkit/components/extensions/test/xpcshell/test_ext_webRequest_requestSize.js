"use strict";

const server = createHttpServer();
const gServerUrl = `http://localhost:${server.identity.primaryPort}`;

const EXTENSION_DATA = {
  manifest: {
    name: "Simple extension test",
    version: "1.0",
    manifest_version: 2,
    description: "",

    permissions: ["webRequest", "<all_urls>"],
  },

  async background() {
    browser.test.log("background script running");

    browser.webRequest.onBeforeSendHeaders.addListener(
      async details => {
        browser.test.assertTrue(details.requestSize == 0, "no requestSize");
        browser.test.assertTrue(details.responseSize == 0, "no responseSize");
        browser.test.log(`details.requestSize: ${details.requestSize}`);
        browser.test.log(`details.responseSize: ${details.responseSize}`);
        browser.test.sendMessage("check");
      },
      { urls: ["*://*/*"] }
    );

    browser.webRequest.onCompleted.addListener(
      async details => {
        browser.test.assertTrue(details.requestSize > 100, "have requestSize");
        browser.test.assertTrue(
          details.responseSize > 100,
          "have responseSize"
        );
        browser.test.log(`details.requestSize: ${details.requestSize}`);
        browser.test.log(`details.responseSize: ${details.responseSize}`);
        browser.test.sendMessage("done");
      },
      { urls: ["*://*/*"] }
    );
  },
};

add_task(async function test_request_response_size() {
  let ext = ExtensionTestUtils.loadExtension(EXTENSION_DATA);
  await ext.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${gServerUrl}/dummy`
  );
  await ext.awaitMessage("check");
  await ext.awaitMessage("done");
  await contentPage.close();
  await ext.unload();
});
