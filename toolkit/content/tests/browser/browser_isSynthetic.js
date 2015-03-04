function waitForLoad(browser) {
  return new Promise(resolve => {
    let wasSynthetic = browser.isSyntheticDocument;

    browser.addProgressListener({
      onLocationChange: function(webProgress, request, location, flags) {
        wasSynthetic = browser.isSyntheticDocument;
      },

      onStateChange: function(webProgress, request, flags, status) {
        let docStop = Ci.nsIWebProgressListener.STATE_IS_NETWORK |
                      Ci.nsIWebProgressListener.STATE_STOP;
        if ((flags & docStop) == docStop) {
          browser.removeProgressListener(this);
          resolve(wasSynthetic);
        }
      },

      QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                             Ci.nsISupportsWeakReference])
    });
  });
}

const FILES = gTestPath.replace("browser_isSynthetic.js", "")
                       .replace("chrome://mochitests/content/", "http://example.com/");

add_task(function*() {
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield waitForLoad(browser);

  is(browser.isSyntheticDocument, false, "Should not be synthetic");

  let loadPromise = waitForLoad(browser);
  browser.loadURI("data:text/html;charset=utf-8,<html/>");
  let wasSynthetic = yield loadPromise;
  is(wasSynthetic, false, "Should not be synthetic");
  is(browser.isSyntheticDocument, false, "Should not be synthetic");

  loadPromise = waitForLoad(browser);
  browser.loadURI(FILES + "empty.png");
  wasSynthetic = yield loadPromise;
  is(wasSynthetic, true, "Should be synthetic");
  is(browser.isSyntheticDocument, true, "Should be synthetic");

  loadPromise = waitForLoad(browser);
  browser.goBack();
  wasSynthetic = yield loadPromise;
  is(wasSynthetic, false, "Should not be synthetic");
  is(browser.isSyntheticDocument, false, "Should not be synthetic");

  loadPromise = waitForLoad(browser);
  browser.goForward();
  wasSynthetic = yield loadPromise;
  is(wasSynthetic, true, "Should be synthetic");
  is(browser.isSyntheticDocument, true, "Should be synthetic");

  gBrowser.removeTab(tab);
});
