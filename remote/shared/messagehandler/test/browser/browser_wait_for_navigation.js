/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "https://example.org/document-builder.sjs?html=test";
const TEST_URL_2 = "https://example.com/document-builder.sjs?html=test_2";

const SLOW_IMG = encodeURIComponent(
  `<img src="https://example.org/document-builder.sjs?delay=100000&html=image">`
);
const TEST_URL_3 = "https://example.org/document-builder.sjs?html=" + SLOW_IMG;

const { NavigationStrategy } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm"
);

const { HttpServer } = ChromeUtils.import(
  "chrome://remote/content/server/HTTPD.jsm"
);

add_task(async function test_waitForNavigation_loadURLs() {
  const root = await setupRootMessageHandler();

  const browser = gBrowser.selectedTab.linkedBrowser;
  const webProgress = browser.browsingContext.webProgress;

  info("Navigate the initial tab to the test URL");
  await loadURL(browser, TEST_URL);

  info("Navigate to another normal page");
  const onNavigation = root.waitForNavigation(webProgress);
  BrowserTestUtils.loadURI(browser, TEST_URL_2);
  await onNavigation;
  is(getCurrentURI(browser), TEST_URL_2);

  info("Navigate to a page with a very slow resource");
  const onInteractiveNavigation = root.waitForNavigation(
    webProgress,
    NavigationStrategy.WaitForInteractive
  );
  // This navigation would timeout for the default strategy (WaitForComplete).
  BrowserTestUtils.loadURI(browser, TEST_URL_3);
  await onInteractiveNavigation;
  is(getCurrentURI(browser), TEST_URL_3);

  root.destroy();
});

add_task(async function test_waitForNavigation_backForward() {
  const root = await setupRootMessageHandler();

  const browser = gBrowser.selectedTab.linkedBrowser;
  const webProgress = browser.browsingContext.webProgress;
  info("Navigate the initial tab to the test URL");
  await loadURL(browser, TEST_URL);

  info("Navigate to a second page");
  const onNavigation = root.waitForNavigation(
    browser.browsingContext.webProgress
  );
  BrowserTestUtils.loadURI(browser, TEST_URL_2);
  await onNavigation;

  // Check that all strategies resolve properly
  const strategies = Object.values(NavigationStrategy);

  info("Go back to the first page");
  const onBackPromises = strategies.map(strategy =>
    root.waitForNavigation(webProgress, strategy)
  );
  gBrowser.goBack();
  await Promise.all(onBackPromises);
  is(getCurrentURI(browser), TEST_URL, "Url should be the first test URL");

  info("Go forward to the second page");
  const onForwardPromises = strategies.map(strategy =>
    root.waitForNavigation(webProgress, strategy)
  );
  gBrowser.goForward();
  await Promise.all(onForwardPromises);
  is(getCurrentURI(browser), TEST_URL_2, "Url should be the second test URL");

  root.destroy();
});

add_task(async function test_waitForNavigation_allStrategies() {
  const root = await setupRootMessageHandler();

  const browser = gBrowser.selectedTab.linkedBrowser;
  const webProgress = browser.browsingContext.webProgress;
  info("Navigate the initial tab to the test URL");
  await loadURL(browser, TEST_URL);

  let resolveHtmlRequest;
  const htmlPromise = new Promise(r => (resolveHtmlRequest = r));
  let resolveImgRequest;
  const imgPromise = new Promise(r => (resolveImgRequest = r));

  // Create the server
  const server = new HttpServer();
  registerCleanupFunction(async function cleanup() {
    await new Promise(resolve => server.stop(resolve));
  });
  server.start(-1);

  // Register a path for a controlled HTML page
  server.registerContentType("html", "text/html");
  server.registerPathHandler("/blocked.html", async (request, response) => {
    response.processAsync();
    await htmlPromise;
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`<!DOCTYPE html>
      <html>
          <img src="img.gif">
      </html>
    `);
    response.finish();
  });

  // Register a path for a controlled image
  server.registerContentType("gif", "image/gif");
  server.registerPathHandler("/img.gif", async (request, response) => {
    response.processAsync();
    await imgPromise;
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.finish();
  });

  const BLOCKED_URL = `http://localhost:${server.identity.primaryPort}/blocked.html`;

  info("Create listeners for all navigation steps");
  const onStart = root.waitForNavigation(
    webProgress,
    NavigationStrategy.WaitForStart
  );

  const onInteractive = root.waitForNavigation(
    webProgress,
    NavigationStrategy.WaitForInteractive
  );
  let onInteractiveResolved = false;
  onInteractive.then(() => (onInteractiveResolved = true));

  const onComplete = root.waitForNavigation(
    webProgress,
    NavigationStrategy.WaitForComplete
  );
  let onCompleteResolved = false;
  onComplete.then(() => (onCompleteResolved = true));

  info("Attempt to load the test page and wait for navigation start");
  BrowserTestUtils.loadURI(browser, BLOCKED_URL);
  await onStart;

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));
  is(getCurrentURI(browser), TEST_URL, "Url should still be TEST_URL");
  ok(!onInteractiveResolved, "The interactive promise has not resolved yet");
  ok(!onCompleteResolved, "The complete promise has not resolved yet");

  info("Unblock the initial request and wait for interactive");
  resolveHtmlRequest();
  await onInteractive;
  is(getCurrentURI(browser), BLOCKED_URL, "Url should now be BLOCKED_URL");

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));
  ok(!onCompleteResolved, "The complete promise has not resolved yet");

  info("Unblock the image request and wait for complete");
  resolveImgRequest();
  await onComplete;
  is(getCurrentURI(browser), BLOCKED_URL, "Url should still be BLOCKED_URL");
});

async function setupRootMessageHandler() {
  const rootMessageHandler = createRootMessageHandler(
    "session-id-event_with_frames"
  );

  info("Add a new session data item to get window global handlers created");
  await rootMessageHandler.addSessionData({
    moduleName: "command",
    category: "browser_session_data_browser_element",
    contextDescriptor: {
      type: ContextDescriptorType.All,
    },
    values: [true],
  });

  return rootMessageHandler;
}

function getCurrentURI(browser) {
  return browser.browsingContext.currentWindowGlobal.documentURI.displaySpec;
}
