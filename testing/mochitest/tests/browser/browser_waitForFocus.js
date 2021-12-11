const gBaseURL = "https://example.com/browser/testing/mochitest/tests/browser/";

function promiseTabLoadEvent(tab, url) {
  let promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, url);
  if (url) {
    tab.linkedBrowser.loadURI(url);
  }
  return promise;
}

// Load a new blank tab
add_task(async function() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser);

  gURLBar.focus();

  let browser = gBrowser.selectedBrowser;
  await SimpleTest.promiseFocus(browser, true);

  is(
    document.activeElement,
    browser,
    "Browser is focused when about:blank is loaded"
  );

  gBrowser.removeCurrentTab();
  gURLBar.focus();
});

add_task(async function() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser);

  gURLBar.focus();

  let browser = gBrowser.selectedBrowser;
  // If we're running in e10s, we don't have access to the content
  // window, so only test window arguments in non-e10s mode.
  if (browser.contentWindow) {
    await SimpleTest.promiseFocus(browser.contentWindow, true);

    is(
      document.activeElement,
      browser,
      "Browser is focused when about:blank is loaded"
    );
  }

  gBrowser.removeCurrentTab();
  gURLBar.focus();
});

// Load a tab with a subframe inside it and wait until the subframe is focused
add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  // If we're running in e10s, we don't have access to the content
  // window, so only test <iframe> arguments in non-e10s mode.
  if (browser.contentWindow) {
    await promiseTabLoadEvent(tab, gBaseURL + "waitForFocusPage.html");

    await SimpleTest.promiseFocus(browser.contentWindow);

    is(
      document.activeElement,
      browser,
      "Browser is focused when page is loaded"
    );

    await SimpleTest.promiseFocus(browser.contentWindow.frames[0]);

    is(
      browser.contentWindow.document.activeElement.localName,
      "iframe",
      "Child iframe is focused"
    );
  }

  gBrowser.removeCurrentTab();
});

// Pass a browser to promiseFocus
add_task(async function() {
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    gBaseURL + "waitForFocusPage.html"
  );

  gURLBar.focus();

  await SimpleTest.promiseFocus(gBrowser.selectedBrowser);

  is(
    document.activeElement,
    gBrowser.selectedBrowser,
    "Browser is focused when promiseFocus is passed a browser"
  );

  gBrowser.removeCurrentTab();
});

// Tests focusing the sidebar, which is in a parent process subframe
// and then switching the focus to another window.
add_task(async function() {
  await SidebarUI.show("viewBookmarksSidebar");

  gURLBar.focus();

  // Focus the sidebar.
  await SimpleTest.promiseFocus(SidebarUI.browser);
  is(
    document.activeElement,
    document.getElementById("sidebar"),
    "sidebar focused"
  );
  ok(
    document.activeElement.contentDocument.hasFocus(),
    "sidebar document hasFocus"
  );

  // Focus the sidebar again, which should cause no change.
  await SimpleTest.promiseFocus(SidebarUI.browser);
  is(
    document.activeElement,
    document.getElementById("sidebar"),
    "sidebar focused"
  );
  ok(
    document.activeElement.contentDocument.hasFocus(),
    "sidebar document hasFocus"
  );

  // Focus another window. The sidebar should no longer be focused.
  let window2 = await BrowserTestUtils.openNewBrowserWindow();
  is(
    document.activeElement,
    document.getElementById("sidebar"),
    "sidebar focused after window 2 opened"
  );
  ok(
    !document.activeElement.contentDocument.hasFocus(),
    "sidebar document hasFocus after window 2 opened"
  );

  // Focus the first window again and the sidebar should be focused again.
  await SimpleTest.promiseFocus(window);
  is(
    document.activeElement,
    document.getElementById("sidebar"),
    "sidebar focused after window1 refocused"
  );
  ok(
    document.activeElement.contentDocument.hasFocus(),
    "sidebar document hasFocus after window1 refocused"
  );

  await BrowserTestUtils.closeWindow(window2);
  await SidebarUI.hide();
});
