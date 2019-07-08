ChromeUtils.defineModuleGetter(
  this,
  "Promise",
  "resource://gre/modules/Promise.jsm"
);

/* exported promiseTabLoadEvent, is_element_visible, is_element_hidden */

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param tab
 *        The tab to load into.
 * @param [optional] url
 *        The url to load, or the current url.
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url) {
  let deferred = Promise.defer();
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  // Create two promises: one resolved from the content process when the page
  // loads and one that is rejected if we take too long to load the url.
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  let timeout = setTimeout(() => {
    deferred.reject(new Error("Timed out while waiting for a 'load' event"));
  }, 30000);

  loaded.then(() => {
    clearTimeout(timeout);
    deferred.resolve();
  });

  if (url) {
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);
  }

  // Promise.all rejects if either promise rejects (i.e. if we time out) and
  // if our loaded promise resolves before the timeout, then we resolve the
  // timeout promise as well, causing the all promise to resolve.
  return Promise.all([deferred.promise, loaded]);
}

function is_element_visible(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_visible(element), msg || "Element should be visible");
}
function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_hidden(element), msg || "Element should be hidden");
}
