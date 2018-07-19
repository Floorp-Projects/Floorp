let testURL = "https://example.com/browser/" +
  "uriloader/exthandler/tests/mochitest/protocolHandler.html";

add_task(async function() {
  // Load a page registering a protocol handler.
  let browser = gBrowser.selectedBrowser;
  browser.loadURI(testURL);
  await BrowserTestUtils.browserLoaded(browser, false, testURL);

  // Register the protocol handler by clicking the notificationbar button.
  let notificationValue = "Protocol Registration: web+testprotocol";
  let getNotification = () =>
    gBrowser.getNotificationBox().getNotificationWithValue(notificationValue);
  await BrowserTestUtils.waitForCondition(getNotification);
  let notification = getNotification();
  let button =
    notification.getElementsByClassName("notification-button-default")[0];
  ok(button, "got registration button");
  button.click();

  // Set the new handler as default.
  const protoSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"].
                     getService(Ci.nsIExternalProtocolService);
  let protoInfo = protoSvc.getProtocolHandlerInfo("web+testprotocol");
  is(protoInfo.preferredAction, protoInfo.useHelperApp,
     "using a helper application is the preferred action");
  ok(!protoInfo.preferredApplicationHandler, "no preferred handler is set");
  let handlers = protoInfo.possibleApplicationHandlers;
  is(1, handlers.length, "only one handler registered for web+testprotocol");
  let handler = handlers.queryElementAt(0, Ci.nsIHandlerApp);
  ok(handler instanceof Ci.nsIWebHandlerApp, "the handler is a web handler");
  is(handler.uriTemplate, "https://example.com/foobar?uri=%s",
     "correct url template")
  protoInfo.preferredApplicationHandler = handler;
  protoInfo.alwaysAskBeforeHandling = false;
  const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].
                       getService(Ci.nsIHandlerService);
  handlerSvc.store(protoInfo);

  // Middle-click a testprotocol link and check the new tab is correct
  let link = "#link";
  const expectedURL = "https://example.com/foobar?uri=web%2Btestprotocol%3Atest";

  let promiseTabOpened =
    BrowserTestUtils.waitForNewTab(gBrowser, expectedURL);
  await BrowserTestUtils.synthesizeMouseAtCenter(link, {button: 1}, browser);
  let tab = await promiseTabOpened;
  gBrowser.selectedTab = tab;
  is(gURLBar.value, expectedURL,
     "the expected URL is displayed in the location bar");
  BrowserTestUtils.removeTab(tab);

  // Shift-click the testprotocol link and check the new window.
  let newWindowPromise = BrowserTestUtils.waitForNewWindow();
  await BrowserTestUtils.synthesizeMouseAtCenter(link, {shiftKey: true},
                                                 browser);
  let win = await newWindowPromise;
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  await BrowserTestUtils.waitForCondition(() => win.gBrowser.currentURI.spec == expectedURL);
  is(win.gURLBar.value, expectedURL,
     "the expected URL is displayed in the location bar");
  await BrowserTestUtils.closeWindow(win);

  // Click the testprotocol link and check the url in the current tab.
  let loadPromise = BrowserTestUtils.browserLoaded(browser);
  await BrowserTestUtils.synthesizeMouseAtCenter(link, {}, browser);
  await loadPromise;
  await BrowserTestUtils.waitForCondition(() => gURLBar.value != testURL);
  is(gURLBar.value, expectedURL,
     "the expected URL is displayed in the location bar");

  // Cleanup.
  protoInfo.preferredApplicationHandler = null;
  handlers.removeElementAt(0);
  handlerSvc.store(protoInfo);
});
