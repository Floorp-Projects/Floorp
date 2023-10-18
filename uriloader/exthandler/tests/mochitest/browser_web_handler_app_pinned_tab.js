/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

let testURL =
  "http://mochi.test:8888/browser/" +
  "uriloader/exthandler/tests/mochitest/mailto.html";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gExternalProtocolService",
  "@mozilla.org/uriloader/external-protocol-service;1",
  "nsIExternalProtocolService"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "gHandlerService",
  "@mozilla.org/uriloader/handler-service;1",
  "nsIHandlerService"
);

let prevAlwaysAskBeforeHandling;
let prevPreferredAction;
let prevPreferredApplicationHandler;

add_setup(async function () {
  let handler = gExternalProtocolService.getProtocolHandlerInfo("mailto", {});

  // Create a fake mail handler
  const APP_NAME = "ExMail";
  const HANDLER_URL = "https://example.com/?extsrc=mailto&url=%s";
  let app = Cc["@mozilla.org/uriloader/web-handler-app;1"].createInstance(
    Ci.nsIWebHandlerApp
  );
  app.uriTemplate = HANDLER_URL;
  app.name = APP_NAME;

  // Store defaults
  prevAlwaysAskBeforeHandling = handler.alwaysAskBeforeHandling;
  prevPreferredAction = handler.preferredAction;
  prevPreferredApplicationHandler = handler.preferredApplicationHandler;

  // Set the fake app as default
  handler.alwaysAskBeforeHandling = false;
  handler.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  handler.preferredApplicationHandler = app;
  gHandlerService.store(handler);
});

registerCleanupFunction(async function () {
  let handler = gExternalProtocolService.getProtocolHandlerInfo("mailto", {});
  handler.alwaysAskBeforeHandling = prevAlwaysAskBeforeHandling;
  handler.preferredAction = prevPreferredAction;
  handler.preferredApplicationHandler = prevPreferredApplicationHandler;
  gHandlerService.store(handler);
});

add_task(async function () {
  const expectedURL =
    "https://example.com/?extsrc=mailto&url=mailto%3Amail%40example.com";

  // Load a page with mailto handler.
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, testURL);
  await BrowserTestUtils.browserLoaded(browser, false, testURL);

  // Pin as an app tab
  gBrowser.pinTab(gBrowser.selectedTab);

  // Click the link and check the new tab is correct
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, expectedURL);
  await BrowserTestUtils.synthesizeMouseAtCenter("#link", {}, browser);
  let tab = await promiseTabOpened;
  is(
    gURLBar.value,
    UrlbarTestUtils.trimURL(expectedURL),
    "the mailto web handler is opened in a new tab"
  );
  BrowserTestUtils.removeTab(tab);
});
