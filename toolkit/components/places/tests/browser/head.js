ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
});

const TRANSITION_LINK = PlacesUtils.history.TRANSITION_LINK;
const TRANSITION_TYPED = PlacesUtils.history.TRANSITION_TYPED;
const TRANSITION_BOOKMARK = PlacesUtils.history.TRANSITION_BOOKMARK;
const TRANSITION_REDIRECT_PERMANENT =
  PlacesUtils.history.TRANSITION_REDIRECT_PERMANENT;
const TRANSITION_REDIRECT_TEMPORARY =
  PlacesUtils.history.TRANSITION_REDIRECT_TEMPORARY;
const TRANSITION_EMBED = PlacesUtils.history.TRANSITION_EMBED;
const TRANSITION_FRAMED_LINK = PlacesUtils.history.TRANSITION_FRAMED_LINK;
const TRANSITION_DOWNLOAD = PlacesUtils.history.TRANSITION_DOWNLOAD;

function whenNewWindowLoaded(aOptions, aCallback) {
  BrowserTestUtils.waitForNewWindow().then(aCallback);
  OpenBrowserWindow(aOptions);
}

async function clearHistoryAndHistoryCache() {
  await PlacesUtils.history.clear();
  // Clear HistoryRestiction cache as well.
  Cc["@mozilla.org/browser/history;1"]
    .getService(Ci.mozIAsyncHistory)
    .clearCache();
}

async function synthesizeVisitByUser(browser, url) {
  let onNewTab = BrowserTestUtils.waitForNewTab(browser.ownerGlobal.gBrowser);
  // We intentionally turn off this a11y check, because the following click is
  // purposefully sent on an arbitrary web content that is not expected to be
  // tested by itself with the browser mochitests, therefore this rule check
  // shall be ignored by a11y_checks suite.
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
  await ContentTask.spawn(browser, [url], async ([href]) => {
    EventUtils.synthesizeMouseAtCenter(
      content.document.querySelector(`a[href='${href}'`),
      {},
      content
    );
  });
  AccessibilityUtils.resetEnv();
  let tab = await onNewTab;
  BrowserTestUtils.removeTab(tab);
}

async function synthesizeVisitByScript(browser, url) {
  let onNewTab = BrowserTestUtils.waitForNewTab(browser.ownerGlobal.gBrowser);
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
  await ContentTask.spawn(browser, [url], async ([href]) => {
    let a = content.document.querySelector(`a[href='${href}'`);
    a.click();
  });
  AccessibilityUtils.resetEnv();
  let tab = await onNewTab;
  BrowserTestUtils.removeTab(tab);
}

async function assertLinkVisitedStatus(
  browser,
  url,
  { visitCount: expectedVisitCount, isVisited: expectedVisited }
) {
  await BrowserTestUtils.waitForCondition(async () => {
    let visitCount =
      (await PlacesTestUtils.getDatabaseValue("moz_places", "visit_count", {
        url,
      })) ?? 0;

    if (visitCount != expectedVisitCount) {
      return false;
    }

    Assert.equal(visitCount, expectedVisitCount, "The visit count is correct");
    return true;
  });

  await ContentTask.spawn(
    browser,
    [url, expectedVisited],
    async ([href, visited]) => {
      // ElementState::VISITED
      const VISITED_STATE = 1 << 18;
      await ContentTaskUtils.waitForCondition(() => {
        let isVisited = !!(
          content.InspectorUtils.getContentState(
            content.document.querySelector(`a[href='${href}']`)
          ) & VISITED_STATE
        );
        return isVisited == visited;
      });
    }
  );
  Assert.ok(true, "The visited state is corerct");
}
