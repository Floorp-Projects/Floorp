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

  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener,
                                          Ci.nsISupportsWeakReference])
};

const FILES = gTestPath.replace("browser_isSynthetic.js", "")
                       .replace("chrome://mochitests/content/", "http://example.com/");

function waitForPageShow(browser) {
  return ContentTask.spawn(browser, null, async function() {
    ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");
    await new Promise(resolve => {
      let listener = () => {
        removeEventListener("pageshow", listener, true);
        resolve();
      };
      addEventListener("pageshow", listener, true);
    });
  });
}

add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);
  let listener = new LocationChangeListener(browser);

  is(browser.isSyntheticDocument, false, "Should not be synthetic");

  let loadPromise = waitForPageShow(browser);
  browser.loadURI("data:text/html;charset=utf-8,<html/>");
  await loadPromise;
  is(listener.wasSynthetic, false, "Should not be synthetic");
  is(browser.isSyntheticDocument, false, "Should not be synthetic");

  loadPromise = waitForPageShow(browser);
  browser.loadURI(FILES + "empty.png");
  await loadPromise;
  is(listener.wasSynthetic, true, "Should be synthetic");
  is(browser.isSyntheticDocument, true, "Should be synthetic");

  loadPromise = waitForPageShow(browser);
  browser.goBack();
  await loadPromise;
  is(listener.wasSynthetic, false, "Should not be synthetic");
  is(browser.isSyntheticDocument, false, "Should not be synthetic");

  loadPromise = waitForPageShow(browser);
  browser.goForward();
  await loadPromise;
  is(listener.wasSynthetic, true, "Should be synthetic");
  is(browser.isSyntheticDocument, true, "Should be synthetic");

  listener.destroy();
  gBrowser.removeTab(tab);
});
