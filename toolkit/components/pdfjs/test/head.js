async function waitForPdfJS(browser, url) {
  // Runs tests after all "load" event handlers have fired off
  let loadPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "documentloaded",
    false,
    null,
    true
  );
  await SpecialPowers.spawn(browser, [url], contentUrl => {
    content.location = contentUrl;
  });
  return loadPromise;
}

async function waitForPdfJSAnnotationLayer(browser, url) {
  let loadPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "annotationlayerrendered",
    false,
    null,
    true
  );
  await SpecialPowers.spawn(browser, [url], contentUrl => {
    content.location = contentUrl;
  });
  return loadPromise;
}

async function waitForPdfJSSandbox(browser) {
  let loadPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "sandboxcreated",
    false,
    null,
    true
  );
  return loadPromise;
}

function changeMimeHandler(preferredAction, alwaysAskBeforeHandling) {
  let handlerService = Cc[
    "@mozilla.org/uriloader/handler-service;1"
  ].getService(Ci.nsIHandlerService);
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );
  var oldAction = [
    handlerInfo.preferredAction,
    handlerInfo.alwaysAskBeforeHandling,
  ];

  // Change and save mime handler settings
  handlerInfo.alwaysAskBeforeHandling = alwaysAskBeforeHandling;
  handlerInfo.preferredAction = preferredAction;
  handlerService.store(handlerInfo);

  // Refresh data
  handlerInfo = mimeService.getFromTypeAndExtension("application/pdf", "pdf");

  // Test: Mime handler was updated
  is(
    handlerInfo.alwaysAskBeforeHandling,
    alwaysAskBeforeHandling,
    "always-ask prompt change successful"
  );
  is(
    handlerInfo.preferredAction,
    preferredAction,
    "mime handler change successful"
  );

  return oldAction;
}
