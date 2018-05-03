/* eslint-disable mozilla/no-arbitrary-setTimeout */
ChromeUtils.import("resource://gre/modules/Timer.jsm", this);

const TEST_PAGE_URI = "data:text/html;charset=utf-8,The letter s.";
// Using 'javascript' schema to bypass E10SUtils.canLoadURIInProcess, because
// it does not allow 'data:' URI to be loaded in the parent process.
const E10S_PARENT_TEST_PAGE_URI = "javascript:document.write('The letter s.');";

/**
 * Makes sure that the findbar hotkeys (' and /) event listeners
 * are added to the system event group and do not get blocked
 * by calling stopPropagation on a keypress event on a page.
 */
add_task(async function test_hotkey_event_propagation() {
  info("Ensure hotkeys are not affected by stopPropagation.");

  // Opening new tab
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE_URI);
  let browser = gBrowser.getBrowserForTab(tab);
  let findbar = await gBrowser.getFindBar();

  // Pressing these keys open the findbar.
  const HOTKEYS = ["/", "'"];

  // Checking if findbar appears when any hotkey is pressed.
  for (let key of HOTKEYS) {
    is(findbar.hidden, true, "Findbar is hidden now.");
    gBrowser.selectedTab = tab;
    await SimpleTest.promiseFocus(gBrowser.selectedBrowser);
    await BrowserTestUtils.sendChar(key, browser);
    is(findbar.hidden, false, "Findbar should not be hidden.");
    await closeFindbarAndWait(findbar);
  }

  // Stop propagation for all keyboard events.
  let frameScript = () => {
    const stopPropagation = e => e.stopImmediatePropagation();
    let window = content.document.defaultView;
    window.removeEventListener("keydown", stopPropagation);
    window.removeEventListener("keypress", stopPropagation);
    window.removeEventListener("keyup", stopPropagation);
  };

  let mm = browser.messageManager;
  mm.loadFrameScript("data:,(" + frameScript.toString() + ")();", false);

  // Checking if findbar still appears when any hotkey is pressed.
  for (let key of HOTKEYS) {
    is(findbar.hidden, true, "Findbar is hidden now.");
    gBrowser.selectedTab = tab;
    await SimpleTest.promiseFocus(gBrowser.selectedBrowser);
    await BrowserTestUtils.sendChar(key, browser);
    is(findbar.hidden, false, "Findbar should not be hidden.");
    await closeFindbarAndWait(findbar);
  }

  gBrowser.removeTab(tab);
});

add_task(async function test_not_found() {
  info("Check correct 'Phrase not found' on new tab");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE_URI);

  // Search for the first word.
  await promiseFindFinished("--- THIS SHOULD NEVER MATCH ---", false);
  let findbar = gBrowser.getCachedFindBar();
  is(findbar._findStatusDesc.textContent, findbar._notFoundStr,
     "Findbar status text should be 'Phrase not found'");

  gBrowser.removeTab(tab);
});

add_task(async function test_found() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE_URI);

  // Search for a string that WILL be found, with 'Highlight All' on
  await promiseFindFinished("S", true);
  ok(!gBrowser.getCachedFindBar()._findStatusDesc.textContent,
     "Findbar status should be empty");

  gBrowser.removeTab(tab);
});

// Setting first findbar to case-sensitive mode should not affect
// new tab find bar.
add_task(async function test_tabwise_case_sensitive() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE_URI);
  let findbar1 = await gBrowser.getFindBar();

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE_URI);
  let findbar2 = await gBrowser.getFindBar();

  // Toggle case sensitivity for first findbar
  findbar1.getElement("find-case-sensitive").click();

  gBrowser.selectedTab = tab1;

  // Not found for first tab.
  await promiseFindFinished("S", true);
  is(findbar1._findStatusDesc.textContent, findbar1._notFoundStr,
     "Findbar status text should be 'Phrase not found'");

  gBrowser.selectedTab = tab2;

  // But it didn't affect the second findbar.
  await promiseFindFinished("S", true);
  ok(!findbar2._findStatusDesc.textContent, "Findbar status should be empty");

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
});

/**
 * Navigating from a web page (for example mozilla.org) to an internal page
 * (like about:addons) might trigger a change of browser's remoteness.
 * 'Remoteness change' means that rendering page content moves from child
 * process into the parent process or the other way around.
 * This test ensures that findbar properly handles such a change.
 */
add_task(async function test_reinitialization_at_remoteness_change() {
  // This test only makes sence in e10s evironment.
  if (!gMultiProcessBrowser) {
    info("Skipping this test because of non-e10s environment.");
    return;
  }

  info("Ensure findbar re-initialization at remoteness change.");

  // Load a remote page and trigger findbar construction.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE_URI);
  let browser = gBrowser.getBrowserForTab(tab);
  let findbar = await gBrowser.getFindBar();

  // Findbar should operate normally.
  await promiseFindFinished("z", false);
  is(findbar._findStatusDesc.textContent, findbar._notFoundStr,
     "Findbar status text should be 'Phrase not found'");

  await promiseFindFinished("s", false);
  ok(!findbar._findStatusDesc.textContent, "Findbar status should be empty");

  // Moving browser into the parent process and reloading sample data.
  ok(browser.isRemoteBrowser, "Browser should be remote now.");
  await promiseRemotenessChange(tab, false);
  await BrowserTestUtils.loadURI(browser, E10S_PARENT_TEST_PAGE_URI);
  ok(!browser.isRemoteBrowser, "Browser should not be remote any more.");

  // Findbar should keep operating normally after remoteness change.
  await promiseFindFinished("z", false);
  is(findbar._findStatusDesc.textContent, findbar._notFoundStr,
     "Findbar status text should be 'Phrase not found'");

  await promiseFindFinished("s", false);
  ok(!findbar._findStatusDesc.textContent, "Findbar status should be empty");

  BrowserTestUtils.removeTab(tab);
});

/**
 * Ensure that the initial typed characters aren't lost immediately after
 * opening the find bar.
 */
add_task(async function e10sLostKeys() {
  // This test only makes sence in e10s evironment.
  if (!gMultiProcessBrowser) {
    info("Skipping this test because of non-e10s environment.");
    return;
  }

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE_URI);

  ok(!gFindBarInitialized, "findbar isn't initialized yet");

  await gFindBarPromise;
  let findBar = gFindBar;
  let initialValue = findBar._findField.value;

  await EventUtils.synthesizeAndWaitKey("f", { accelKey: true }, window, null, () => {
    // We can't afford to wait for the promise to resolve, by then the
    // find bar is visible and focused, so sending characters to the
    // content browser wouldn't work.
    isnot(document.activeElement, findBar._findField.inputField,
      "findbar is not yet focused");
    EventUtils.synthesizeKey("a");
    EventUtils.synthesizeKey("b");
    EventUtils.synthesizeKey("c");
    is(findBar._findField.value, initialValue, "still has initial find query");
  });

  await BrowserTestUtils.waitForCondition(() => findBar._findField.value.length == 3);
  is(document.activeElement, findBar._findField.inputField,
    "findbar is now focused");
  is(findBar._findField.value, "abc", "abc fully entered as find query");

  BrowserTestUtils.removeTab(tab);
});

function promiseFindFinished(searchText, highlightOn) {
  return new Promise(async (resolve) => {

    let findbar = await gBrowser.getFindBar();
    findbar.startFind(findbar.FIND_NORMAL);
    let highlightElement = findbar.getElement("highlight");
    if (highlightElement.checked != highlightOn)
      highlightElement.click();
    executeSoon(() => {
      findbar._findField.value = searchText;

      let resultListener;
      // When highlighting is on the finder sends a second "FOUND" message after
      // the search wraps. This causes timing problems with e10s. waitMore
      // forces foundOrTimeout wait for the second "FOUND" message before
      // resolving the promise.
      let waitMore = highlightOn;
      let findTimeout = setTimeout(() => foundOrTimedout(null), 2000);
      let foundOrTimedout = function(aData) {
        if (aData !== null && waitMore) {
          waitMore = false;
          return;
        }
        if (aData === null)
          info("Result listener not called, timeout reached.");
        clearTimeout(findTimeout);
        findbar.browser.finder.removeResultListener(resultListener);
        resolve();
      };

      resultListener = {
        onFindResult: foundOrTimedout,
        onCurrentSelection() {},
        onMatchesCountResult() {},
        onHighlightFinished() {},
      };
      findbar.browser.finder.addResultListener(resultListener);
      findbar._find();
    });

  });
}

function promiseRemotenessChange(tab, shouldBeRemote) {
  return new Promise((resolve) => {
    let browser = gBrowser.getBrowserForTab(tab);
    tab.addEventListener("TabRemotenessChange", function() {
      resolve();
    }, {once: true});
    gBrowser.updateBrowserRemoteness(browser, shouldBeRemote);
  });
}
