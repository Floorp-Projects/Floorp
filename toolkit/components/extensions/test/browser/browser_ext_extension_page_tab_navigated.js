/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

// The test tasks in this test file tends to trigger an intermittent
// exception raised from JSActor::AfterDestroy, because of a race between
// when the WebExtensions API event is being emitted from the parent process
// and the navigation triggered on the test extension pages.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Actor 'Conduits' destroyed before query 'RunListener' was resolved/
);

AddonTestUtils.initMochitest(this);

const server = AddonTestUtils.createHttpServer({
  hosts: ["example.com", "anotherwebpage.org"],
});

server.registerPathHandler("/", (request, response) => {
  response.write(`<!DOCTYPE html>
    <html>
      <head>
       <meta charset="utf-8">
       <title>test webpage</title>
      </head>
    </html>
  `);
});

function createTestExtPage({ script }) {
  return `<!DOCTYPE html>
    <html>
      <head>
       <meta charset="utf-8">
       <script src="${script}"></script>
      </head>
    </html>
  `;
}

function createTestExtPageScript(name) {
  return `(${function (pageName) {
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        browser.test.log(
          `Extension page "${pageName}" got a webRequest event: ${details.url}`
        );
        browser.test.sendMessage(`event-received:${pageName}`);
      },
      { types: ["main_frame"], urls: ["http://example.com/*"] }
    );
    /* eslint-disable mozilla/balanced-listeners */
    window.addEventListener("pageshow", () => {
      browser.test.log(`Extension page "${pageName}" got a pageshow event`);
      browser.test.sendMessage(`pageshow:${pageName}`);
    });
    window.addEventListener("pagehide", () => {
      browser.test.log(`Extension page "${pageName}" got a pagehide event`);
      browser.test.sendMessage(`pagehide:${pageName}`);
    });
    /* eslint-enable mozilla/balanced-listeners */
  }})("${name}");`;
}

// Triggers a WebRequest listener registered by the test extensions by
// opening a tab on the given web page URL and then closing it after
// it did load.
async function triggerWebRequestListener(webPageURL, pause) {
  let webPageTab = await BrowserTestUtils.openNewForegroundTab(
    {
      gBrowser,
      url: webPageURL,
    },
    true /* waitForLoad */,
    true /* waitForStop */
  );
  BrowserTestUtils.removeTab(webPageTab);
}

// The following tests tasks are testing the expected behaviors related to same-process and cross-process
// navigations for an extension page, similarly to test_ext_extension_page_navigated.js, but unlike its
// xpcshell counterpart this tests are only testing that after navigating back to an extension page
// previously stored in the BFCache the WebExtensions events subscribed are being received as expected.

add_task(async function test_extension_page_sameprocess_navigation() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "http://example.com/*"],
    },
    files: {
      "extpage1.html": createTestExtPage({ script: "extpage1.js" }),
      "extpage1.js": createTestExtPageScript("extpage1"),
      "extpage2.html": createTestExtPage({ script: "extpage2.js" }),
      "extpage2.js": createTestExtPageScript("extpage2"),
    },
  });

  await extension.startup();

  const policy = WebExtensionPolicy.getByID(extension.id);

  const extPageURL1 = policy.extension.baseURI.resolve("extpage1.html");
  const extPageURL2 = policy.extension.baseURI.resolve("extpage2.html");

  info("Opening extension page in a new tab");
  const extPageTab = await BrowserTestUtils.addTab(gBrowser, extPageURL1);
  let browser = gBrowser.getBrowserForTab(extPageTab);
  info("Wait for the extension page to be loaded");
  await extension.awaitMessage("pageshow:extpage1");

  await triggerWebRequestListener("http://example.com");
  await extension.awaitMessage("event-received:extpage1");
  ok(true, "extpage1 got a webRequest event as expected");

  info("Load a second extension page in the same tab");
  BrowserTestUtils.startLoadingURIString(browser, extPageURL2);

  info("Wait extpage1 to receive a pagehide event");
  await extension.awaitMessage("pagehide:extpage1");
  info("Wait extpage2 to receive a pageshow event");
  await extension.awaitMessage("pageshow:extpage2");

  info(
    "Trigger a web request event and expect extpage2 to be the only one receiving it"
  );
  await triggerWebRequestListener("http://example.com");
  await extension.awaitMessage("event-received:extpage2");
  ok(true, "extpage2 got a webRequest event as expected");

  info(
    "Navigating back to extpage1 and expect extpage2 to be the only one receiving the webRequest event"
  );

  browser.goBack();
  info("Wait for extpage1 to receive a pageshow event");
  await extension.awaitMessage("pageshow:extpage1");
  info("Wait for extpage2 to receive a pagehide event");
  await extension.awaitMessage("pagehide:extpage2");

  // We only expect extpage1 to be able to receive API events.
  await triggerWebRequestListener("http://example.com");
  await extension.awaitMessage("event-received:extpage1");
  ok(true, "extpage1 got a webRequest event as expected");

  BrowserTestUtils.removeTab(extPageTab);
  await extension.awaitMessage("pagehide:extpage1");

  await extension.unload();
});

add_task(async function test_extension_page_context_navigated_to_web_page() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "http://example.com/*"],
    },
    files: {
      "extpage.html": createTestExtPage({ script: "extpage.js" }),
      "extpage.js": createTestExtPageScript("extpage"),
    },
  });

  await extension.startup();

  const policy = WebExtensionPolicy.getByID(extension.id);

  const extPageURL = policy.extension.baseURI.resolve("extpage.html");
  // NOTE: this test will navigate the extension page to a webpage url that
  // isn't matching the match pattern the test extension is going to use
  // in its webRequest event listener, otherwise the extension page being
  // navigated will be intermittently able to receive an event before it
  // is navigated to the webpage url (and moved into the BFCache or destroyed)
  // and trigger an intermittent failure of this test.
  const webPageURL = "http://anotherwebpage.org/";
  const triggerWebRequestURL = "http://example.com/";

  info("Opening extension page in a new tab");
  const extPageTab1 = await BrowserTestUtils.addTab(gBrowser, extPageURL);
  let browserForTab1 = gBrowser.getBrowserForTab(extPageTab1);
  info("Wait for the extension page to be loaded");
  await extension.awaitMessage("pageshow:extpage");

  info("Navigate the tab from the extension page to a web page");
  let promiseLoaded = BrowserTestUtils.browserLoaded(
    browserForTab1,
    false,
    webPageURL
  );
  BrowserTestUtils.startLoadingURIString(browserForTab1, webPageURL);
  info("Wait the tab to have loaded the new webpage url");
  await promiseLoaded;
  info("Wait the extension page to receive a pagehide event");
  await extension.awaitMessage("pagehide:extpage");

  // Trigger a webRequest listener, the extension page is expected to
  // not be active, if that isn't the case a test message will be queued
  // and will trigger an explicit test failure.
  await triggerWebRequestListener(triggerWebRequestURL);

  info("Navigate back to the extension page");
  browserForTab1.goBack();
  info("Wait for extension page to receive a pageshow event");
  await extension.awaitMessage("pageshow:extpage");

  await triggerWebRequestListener(triggerWebRequestURL);
  await extension.awaitMessage("event-received:extpage");
  ok(
    true,
    "extpage got a webRequest event as expected after being restored from BFCache"
  );

  info("Cleanup and exit test");
  BrowserTestUtils.removeTab(extPageTab1);

  info("Wait the extension page to receive a pagehide event");
  await extension.awaitMessage("pagehide:extpage");

  await extension.unload();
});
