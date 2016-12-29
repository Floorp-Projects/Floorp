function LocationChangeListener(browser) {
  this.browser = browser;
  browser.addProgressListener(this);
}

LocationChangeListener.prototype = {
  wasSynthetic: false,
  browser: null,

  destroy() {
    this.browser.removeProgressListener(this);
  },

  onLocationChange(webProgress, request, location, flags) {
    this.wasSynthetic = this.browser.isSyntheticDocument;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference])
}

const FILES = gTestPath.replace("browser_isSynthetic.js", "")
                       .replace("chrome://mochitests/content/", "http://example.com/");

function waitForPageShow(browser) {
  return ContentTask.spawn(browser, null, function*() {
    Cu.import("resource://gre/modules/PromiseUtils.jsm");
    yield new Promise(resolve => {
      let listener = () => {
        removeEventListener("pageshow", listener, true);
        resolve();
      }
      addEventListener("pageshow", listener, true);
    });
  });
}

add_task(function*() {
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);
  let listener = new LocationChangeListener(browser);

  is(browser.isSyntheticDocument, false, "Should not be synthetic");

  let loadPromise = waitForPageShow(browser);
  browser.loadURI("data:text/html;charset=utf-8,<html/>");
  yield loadPromise;
  is(listener.wasSynthetic, false, "Should not be synthetic");
  is(browser.isSyntheticDocument, false, "Should not be synthetic");

  loadPromise = waitForPageShow(browser);
  browser.loadURI(FILES + "empty.png");
  yield loadPromise;
  is(listener.wasSynthetic, true, "Should be synthetic");
  is(browser.isSyntheticDocument, true, "Should be synthetic");

  loadPromise = waitForPageShow(browser);
  browser.goBack();
  yield loadPromise;
  is(listener.wasSynthetic, false, "Should not be synthetic");
  is(browser.isSyntheticDocument, false, "Should not be synthetic");

  loadPromise = waitForPageShow(browser);
  browser.goForward();
  yield loadPromise;
  is(listener.wasSynthetic, true, "Should be synthetic");
  is(browser.isSyntheticDocument, true, "Should be synthetic");

  listener.destroy();
  gBrowser.removeTab(tab);
});
