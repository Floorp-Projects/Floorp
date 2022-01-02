const SYSTEMPRINCIPAL = Services.scriptSecurityManager.getSystemPrincipal();
const DUMMY1 =
  "http://test1.example.org/browser/toolkit/modules/tests/browser/dummy_page.html";
const DUMMY2 =
  "http://test2.example.org/browser/toolkit/modules/tests/browser/dummy_page.html";
const LOAD_URI_OPTIONS = { triggeringPrincipal: SYSTEMPRINCIPAL };

function waitForLoad(uri) {
  return BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, uri);
}

function waitForPageShow(browser = gBrowser.selectedBrowser) {
  return BrowserTestUtils.waitForContentEvent(browser, "pageshow", true);
}

// Tests that loadURI accepts a referrer and it is included in the load.
add_task(async function test_referrer() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let browser = gBrowser.selectedBrowser;
  let ReferrerInfo = Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  );

  let loadURIOptionsWithReferrer = {
    triggeringPrincipal: SYSTEMPRINCIPAL,
    referrerInfo: new ReferrerInfo(
      Ci.nsIReferrerInfo.EMPTY,
      true,
      Services.io.newURI(DUMMY2)
    ),
  };
  browser.webNavigation.loadURI(DUMMY1, loadURIOptionsWithReferrer);
  await waitForLoad(DUMMY1);

  await SpecialPowers.spawn(browser, [[DUMMY1, DUMMY2]], function([
    dummy1,
    dummy2,
  ]) {
    function getExpectedReferrer(referrer) {
      let defaultPolicy = Services.prefs.getIntPref(
        "network.http.referer.defaultPolicy"
      );
      ok(
        [2, 3].indexOf(defaultPolicy) > -1,
        "default referrer policy should be either strict-origin-when-cross-origin(2) or no-referrer-when-downgrade(3)"
      );
      if (defaultPolicy == 2) {
        return referrer.match(/https?:\/\/[^\/]+\/?/i)[0];
      }
      return referrer;
    }

    is(content.location.href, dummy1, "Should have loaded the right URL");
    is(
      content.document.referrer,
      getExpectedReferrer(dummy2),
      "Should have the right referrer"
    );
  });

  gBrowser.removeCurrentTab();
});

// Tests that remote access to webnavigation.sessionHistory works.
add_task(async function test_history() {
  async function checkHistoryIndex(browser, n) {
    if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
      return SpecialPowers.spawn(browser, [n], function(n) {
        let history =
          docShell.browsingContext.childSessionHistory.legacySHistory;

        is(history.index, n, "Should be at the right place in history");
      });
    }

    let history = browser.browsingContext.sessionHistory;
    is(history.index, n, "Should be at the right place in history");
    return null;
  }

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let browser = gBrowser.selectedBrowser;

  browser.webNavigation.loadURI(DUMMY1, LOAD_URI_OPTIONS);
  await waitForLoad(DUMMY1);

  browser.webNavigation.loadURI(DUMMY2, LOAD_URI_OPTIONS);
  await waitForLoad(DUMMY2);

  if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
    await SpecialPowers.spawn(browser, [[DUMMY1, DUMMY2]], function([
      dummy1,
      dummy2,
    ]) {
      let history = docShell.browsingContext.childSessionHistory.legacySHistory;

      is(history.count, 2, "Should be two history items");
      is(history.index, 1, "Should be at the right place in history");
      let entry = history.getEntryAtIndex(0);
      is(entry.URI.spec, dummy1, "Should have the right history entry");
      entry = history.getEntryAtIndex(1);
      is(entry.URI.spec, dummy2, "Should have the right history entry");
    });
  } else {
    let history = browser.browsingContext.sessionHistory;

    is(history.count, 2, "Should be two history items");
    is(history.index, 1, "Should be at the right place in history");
    let entry = history.getEntryAtIndex(0);
    is(entry.URI.spec, DUMMY1, "Should have the right history entry");
    entry = history.getEntryAtIndex(1);
    is(entry.URI.spec, DUMMY2, "Should have the right history entry");
  }

  let promise = waitForPageShow();
  browser.webNavigation.goBack();
  await promise;
  await checkHistoryIndex(browser, 0);

  promise = waitForPageShow();
  browser.webNavigation.goForward();
  await promise;
  await checkHistoryIndex(browser, 1);

  promise = waitForPageShow();
  browser.webNavigation.gotoIndex(0);
  await promise;
  await checkHistoryIndex(browser, 0);

  gBrowser.removeCurrentTab();
});

// Tests that load flags are passed through to the content process.
add_task(async function test_flags() {
  async function checkHistory(browser, { count, index }) {
    if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
      return SpecialPowers.spawn(browser, [[DUMMY2, count, index]], function([
        dummy2,
        count,
        index,
      ]) {
        let history =
          docShell.browsingContext.childSessionHistory.legacySHistory;
        is(history.count, count, "Should be one history item");
        is(history.index, index, "Should be at the right place in history");
        let entry = history.getEntryAtIndex(index);
        is(entry.URI.spec, dummy2, "Should have the right history entry");
      });
    }

    let history = browser.browsingContext.sessionHistory;
    is(history.count, count, "Should be one history item");
    is(history.index, index, "Should be at the right place in history");
    let entry = history.getEntryAtIndex(index);
    is(entry.URI.spec, DUMMY2, "Should have the right history entry");

    return null;
  }

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let browser = gBrowser.selectedBrowser;

  browser.webNavigation.loadURI(DUMMY1, LOAD_URI_OPTIONS);
  await waitForLoad(DUMMY1);
  let loadURIOptionsReplaceHistory = {
    triggeringPrincipal: SYSTEMPRINCIPAL,
    loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY,
  };
  browser.webNavigation.loadURI(DUMMY2, loadURIOptionsReplaceHistory);
  await waitForLoad(DUMMY2);
  await checkHistory(browser, { count: 1, index: 0 });
  let loadURIOptionsBypassHistory = {
    triggeringPrincipal: SYSTEMPRINCIPAL,
    loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY,
  };
  browser.webNavigation.loadURI(DUMMY1, loadURIOptionsBypassHistory);
  await waitForLoad(DUMMY1);
  await checkHistory(browser, { count: 1, index: 0 });

  gBrowser.removeCurrentTab();
});

// Tests that attempts to use unsupported arguments throw an exception.
add_task(async function test_badarguments() {
  if (!gMultiProcessBrowser) {
    return;
  }

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let browser = gBrowser.selectedBrowser;

  try {
    let loadURIOptionsBadPostData = {
      triggeringPrincipal: SYSTEMPRINCIPAL,
      postData: {},
    };
    browser.webNavigation.loadURI(DUMMY1, loadURIOptionsBadPostData);
    ok(
      false,
      "Should have seen an exception from trying to pass some postdata"
    );
  } catch (e) {
    ok(true, "Should have seen an exception from trying to pass some postdata");
  }

  try {
    let loadURIOptionsBadHeader = {
      triggeringPrincipal: SYSTEMPRINCIPAL,
      headers: {},
    };
    browser.webNavigation.loadURI(DUMMY1, loadURIOptionsBadHeader);
    ok(false, "Should have seen an exception from trying to pass some headers");
  } catch (e) {
    ok(true, "Should have seen an exception from trying to pass some headers");
  }

  gBrowser.removeCurrentTab();
});
