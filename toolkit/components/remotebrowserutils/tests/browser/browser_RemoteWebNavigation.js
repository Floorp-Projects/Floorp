var { interfaces: Ci, classes: Cc, utils: Cu, results: Cr } = Components;

const DUMMY1 = "http://example.com/browser/toolkit/modules/tests/browser/dummy_page.html";
const DUMMY2 = "http://example.org/browser/toolkit/modules/tests/browser/dummy_page.html"

function waitForLoad(browser = gBrowser.selectedBrowser) {
  return new Promise(resolve => {
    browser.addEventListener("load", function listener() {
      browser.removeEventListener("load", listener, true);
      resolve();
    }, true);
  });
}

function waitForPageShow(browser = gBrowser.selectedBrowser) {
  return new Promise(resolve => {
    browser.addEventListener("pageshow", function listener() {
      browser.removeEventListener("pageshow", listener, true);
      resolve();
    }, true);
  });
}

function makeURI(url) {
  return Cc["@mozilla.org/network/io-service;1"].
         getService(Ci.nsIIOService).
         newURI(url, null, null);
}

// Tests that loadURI accepts a referrer and it is included in the load.
add_task(function* test_referrer() {
  gBrowser.selectedTab = gBrowser.addTab();
  let browser = gBrowser.selectedBrowser;

  browser.webNavigation.loadURI(DUMMY1,
                                Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                                makeURI(DUMMY2),
                                null, null);
  yield waitForLoad();
  is(browser.contentWindow.location, DUMMY1, "Should have loaded the right URL");
  is(browser.contentDocument.referrer, DUMMY2, "Should have the right referrer");

  gBrowser.removeCurrentTab();
});

// Tests that remote access to webnavigation.sessionHistory works.
add_task(function* test_history() {
  gBrowser.selectedTab = gBrowser.addTab();
  let browser = gBrowser.selectedBrowser;

  browser.webNavigation.loadURI(DUMMY1,
                                Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                                null, null, null);
  yield waitForLoad();

  browser.webNavigation.loadURI(DUMMY2,
                                Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                                null, null, null);
  yield waitForLoad();

  let history = browser.webNavigation.sessionHistory;
  is(history.count, 2, "Should be two history items");
  is(history.index, 1, "Should be at the right place in history");
  let entry = history.getEntryAtIndex(0, false);
  is(entry.URI.spec, DUMMY1, "Should have the right history entry");
  entry = history.getEntryAtIndex(1, false);
  is(entry.URI.spec, DUMMY2, "Should have the right history entry");

  let promise = waitForPageShow();
  browser.webNavigation.goBack();
  yield promise;
  is(history.index, 0, "Should be at the right place in history");

  promise = waitForPageShow();
  browser.webNavigation.goForward();
  yield promise;
  is(history.index, 1, "Should be at the right place in history");

  promise = waitForPageShow();
  browser.webNavigation.gotoIndex(0);
  yield promise;
  is(history.index, 0, "Should be at the right place in history");

  gBrowser.removeCurrentTab();
});

// Tests that load flags are passed through to the content process.
add_task(function* test_flags() {
  gBrowser.selectedTab = gBrowser.addTab();
  let browser = gBrowser.selectedBrowser;

  browser.webNavigation.loadURI(DUMMY1,
                                Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                                null, null, null);
  yield waitForLoad();

  browser.webNavigation.loadURI(DUMMY2,
                                Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY,
                                null, null, null);
  yield waitForLoad();

  let history = browser.webNavigation.sessionHistory;
  is(history.count, 1, "Should be one history item");
  is(history.index, 0, "Should be at the right place in history");
  let entry = history.getEntryAtIndex(0, false);
  is(entry.URI.spec, DUMMY2, "Should have the right history entry");

  browser.webNavigation.loadURI(DUMMY1,
                                Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY,
                                null, null, null);
  yield waitForLoad();

  is(history.count, 1, "Should still be one history item");
  is(history.index, 0, "Should be at the right place in history");
  entry = history.getEntryAtIndex(0, false);
  is(entry.URI.spec, DUMMY2, "Should have the right history entry");

  gBrowser.removeCurrentTab();
});

// Tests that attempts to use unsupported arguments throw an exception.
add_task(function* test_badarguments() {
  if (!gMultiProcessBrowser)
    return;

  gBrowser.selectedTab = gBrowser.addTab();
  let browser = gBrowser.selectedBrowser;

  try {
    browser.webNavigation.loadURI(DUMMY1,
                                  Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                                  null, {}, null);
    ok(false, "Should have seen an exception from trying to pass some postdata");
  } catch (e) {
    ok(true, "Should have seen an exception from trying to pass some postdata");
  }

  try {
    browser.webNavigation.loadURI(DUMMY1,
                                  Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                                  null, null, {});
    ok(false, "Should have seen an exception from trying to pass some headers");
  } catch (e) {
    ok(true, "Should have seen an exception from trying to pass some headers");
  }

  gBrowser.removeCurrentTab();
});
