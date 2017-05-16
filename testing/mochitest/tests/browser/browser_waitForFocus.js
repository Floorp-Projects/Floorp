
const gBaseURL = "https://example.com/browser/testing/mochitest/tests/browser/";

function *promiseTabLoadEvent(tab, url)
{
  return new Promise(function (resolve, reject) {
    function handleLoadEvent(event) {
      if (event.originalTarget != tab.linkedBrowser.contentDocument ||
          event.target.location.href == "about:blank" ||
          (url && event.target.location.href != url)) {
        return;
      }

      tab.linkedBrowser.removeEventListener("load", handleLoadEvent, true);
      resolve(event);
    }

    tab.linkedBrowser.addEventListener("load", handleLoadEvent, true, true);
    if (url)
      tab.linkedBrowser.loadURI(url);
  });
}

// Load a new blank tab
add_task(function *() {
  yield BrowserTestUtils.openNewForegroundTab(gBrowser);

  gURLBar.focus();

  let browser = gBrowser.selectedBrowser;
  yield SimpleTest.promiseFocus(browser.contentWindowAsCPOW, true);

  is(document.activeElement, browser, "Browser is focused when about:blank is loaded");

  gBrowser.removeCurrentTab();
  gURLBar.focus();
});

// Load a tab with a subframe inside it and wait until the subframe is focused
add_task(function *() {
  let tab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  yield promiseTabLoadEvent(tab, gBaseURL + "waitForFocusPage.html");

  yield SimpleTest.promiseFocus(browser.contentWindowAsCPOW);

  is(document.activeElement, browser, "Browser is focused when page is loaded");

  yield SimpleTest.promiseFocus(browser.contentWindowAsCPOW.frames[0]);

  is(browser.contentWindowAsCPOW.document.activeElement.localName, "iframe", "Child iframe is focused");

  gBrowser.removeCurrentTab();
});

// Pass a browser to promiseFocus
add_task(function *() {
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, gBaseURL + "waitForFocusPage.html");

  gURLBar.focus();

  yield SimpleTest.promiseFocus(gBrowser.selectedBrowser);

  is(document.activeElement, gBrowser.selectedBrowser, "Browser is focused when promiseFocus is passed a browser");

  gBrowser.removeCurrentTab();
});
