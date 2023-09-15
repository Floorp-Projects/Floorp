/* eslint-disable mozilla/no-arbitrary-setTimeout */

requestLongerTimeout(2);

const TEST_PAGE_URI = "data:text/html;charset=utf-8,The letter s.";
// Using 'javascript' schema to bypass E10SUtils.canLoadURIInRemoteType, because
// it does not allow 'data:' URI to be loaded in the parent process.
const E10S_PARENT_TEST_PAGE_URI =
  getRootDirectory(gTestPath) + "file_empty.html";
const TEST_PAGE_URI_WITHIFRAME =
  "https://example.com/browser/toolkit/content/tests/browser/file_findinframe.html";

/**
 * Makes sure that the findbar hotkeys (' and /) event listeners
 * are added to the system event group and do not get blocked
 * by calling stopPropagation on a keypress event on a page.
 */
add_task(async function test_hotkey_event_propagation() {
  info("Ensure hotkeys are not affected by stopPropagation.");

  // Opening new tab
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE_URI
  );
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
  await SpecialPowers.spawn(browser, [], () => {
    const stopPropagation = e => {
      e.stopImmediatePropagation();
    };
    let window = content.document.defaultView;
    window.addEventListener("keydown", stopPropagation);
    window.addEventListener("keypress", stopPropagation);
    window.addEventListener("keyup", stopPropagation);
  });

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

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE_URI
  );

  // Search for the first word.
  await promiseFindFinished(gBrowser, "--- THIS SHOULD NEVER MATCH ---", false);
  let findbar = gBrowser.getCachedFindBar();
  is(
    findbar._findStatusDesc.dataset.l10nId,
    "findbar-not-found",
    "Findbar status text should be 'Phrase not found'"
  );

  gBrowser.removeTab(tab);
});

add_task(async function test_found() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE_URI
  );

  // Search for a string that WILL be found, with 'Highlight All' on
  await promiseFindFinished(gBrowser, "S", true);
  ok(
    gBrowser.getCachedFindBar()._findStatusDesc.dataset.l10nId === undefined,
    "Findbar status should be empty"
  );

  gBrowser.removeTab(tab);
});

// Setting first findbar to case-sensitive mode should not affect
// new tab find bar.
add_task(async function test_tabwise_case_sensitive() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE_URI
  );
  let findbar1 = await gBrowser.getFindBar();

  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE_URI
  );
  let findbar2 = await gBrowser.getFindBar();

  // Toggle case sensitivity for first findbar
  findbar1.getElement("find-case-sensitive").click();

  gBrowser.selectedTab = tab1;

  // Not found for first tab.
  await promiseFindFinished(gBrowser, "S", true);
  is(
    findbar1._findStatusDesc.dataset.l10nId,
    "findbar-not-found",
    "Findbar status text should be 'Phrase not found'"
  );

  gBrowser.selectedTab = tab2;

  // But it didn't affect the second findbar.
  await promiseFindFinished(gBrowser, "S", true);
  ok(
    findbar2._findStatusDesc.dataset.l10nId === undefined,
    "Findbar status should be empty"
  );

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
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE_URI
  );
  let browser = gBrowser.getBrowserForTab(tab);
  let findbar = await gBrowser.getFindBar();

  // Findbar should operate normally.
  await promiseFindFinished(gBrowser, "z", false);
  is(
    findbar._findStatusDesc.dataset.l10nId,
    "findbar-not-found",
    "Findbar status text should be 'Phrase not found'"
  );

  await promiseFindFinished(gBrowser, "s", false);
  ok(
    findbar._findStatusDesc.dataset.l10nId === undefined,
    "Findbar status should be empty"
  );

  // Moving browser into the parent process and reloading sample data.
  ok(browser.isRemoteBrowser, "Browser should be remote now.");
  await promiseRemotenessChange(tab, false);
  let docLoaded = BrowserTestUtils.browserLoaded(
    browser,
    false,
    E10S_PARENT_TEST_PAGE_URI
  );
  BrowserTestUtils.startLoadingURIString(browser, E10S_PARENT_TEST_PAGE_URI);
  await docLoaded;
  ok(!browser.isRemoteBrowser, "Browser should not be remote any more.");
  browser.contentDocument.body.append("The letter s.");
  browser.contentDocument.body.clientHeight; // Force flush.

  // Findbar should keep operating normally after remoteness change.
  await promiseFindFinished(gBrowser, "z", false);
  is(
    findbar._findStatusDesc.dataset.l10nId,
    "findbar-not-found",
    "Findbar status text should be 'Phrase not found'"
  );

  await promiseFindFinished(gBrowser, "s", false);
  ok(
    findbar._findStatusDesc.dataset.l10nId === undefined,
    "Findbar status should be empty"
  );

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

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE_URI
  );

  ok(!gFindBarInitialized, "findbar isn't initialized yet");

  await gFindBarPromise;
  let findBar = gFindBar;
  let initialValue = findBar._findField.value;

  await EventUtils.synthesizeAndWaitKey(
    "F",
    { accelKey: true },
    window,
    null,
    () => {
      // We can't afford to wait for the promise to resolve, by then the
      // find bar is visible and focused, so sending characters to the
      // content browser wouldn't work.
      isnot(
        document.activeElement,
        findBar._findField,
        "findbar is not yet focused"
      );
      EventUtils.synthesizeKey("a");
      EventUtils.synthesizeKey("b");
      EventUtils.synthesizeKey("c");
      is(
        findBar._findField.value,
        initialValue,
        "still has initial find query"
      );
    }
  );

  await BrowserTestUtils.waitForCondition(
    () => findBar._findField.value.length == 3
  );
  is(document.activeElement, findBar._findField, "findbar is now focused");
  is(findBar._findField.value, "abc", "abc fully entered as find query");

  BrowserTestUtils.removeTab(tab);
});

/**
 * This test makes sure that keyboard operations still occur
 * after the findbar is opened and closed.
 */
add_task(async function test_open_and_close_keys() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html,<body style='height: 5000px;'>Hello There</body>"
  );

  await gFindBarPromise;
  let findBar = gFindBar;

  is(findBar.hidden, true, "Findbar is hidden now.");
  let openedPromise = BrowserTestUtils.waitForEvent(findBar, "findbaropen");
  await EventUtils.synthesizeKey("f", { accelKey: true });
  await openedPromise;

  is(findBar.hidden, false, "Findbar should not be hidden.");

  let closedPromise = BrowserTestUtils.waitForEvent(findBar, "findbarclose");
  await EventUtils.synthesizeKey("KEY_Escape");
  await closedPromise;

  let scrollPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "scroll"
  );
  await EventUtils.synthesizeKey("KEY_ArrowDown");
  await scrollPromise;

  let scrollPosition = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      return content.document.body.scrollTop;
    }
  );

  ok(scrollPosition > 0, "Scrolled ok to " + scrollPosition);

  BrowserTestUtils.removeTab(tab);
});

/**
 * This test makes sure that keyboard navigation (for example arrow up/down,
 * accel+arrow up/down) still works while the findbar is open.
 */
add_task(async function test_input_keypress() {
  await SpecialPowers.pushPrefEnv({ set: [["general.smoothScroll", false]] });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    /* html */
    `data:text/html,
    <!DOCTYPE html>
    <body style='height: 5000px;'>
    Hello There
    </body>`
  );

  await gFindBarPromise;
  let findBar = gFindBar;

  is(findBar.hidden, true, "Findbar is hidden now.");
  let openedPromise = BrowserTestUtils.waitForEvent(findBar, "findbaropen");
  await EventUtils.synthesizeKey("f", { accelKey: true });
  await openedPromise;

  is(findBar.hidden, false, "Findbar should not be hidden.");

  let scrollPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "scroll"
  );
  await EventUtils.synthesizeKey("KEY_ArrowDown");
  await scrollPromise;

  await ContentTask.spawn(tab.linkedBrowser, null, async function () {
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.defaultView.innerHeight +
          content.document.defaultView.pageYOffset >
        0,
      "Scroll with ArrowDown"
    );
  });

  let completeScrollPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "scroll"
  );
  await EventUtils.synthesizeKey("KEY_ArrowDown", { accelKey: true });
  await completeScrollPromise;

  await ContentTask.spawn(tab.linkedBrowser, null, async function () {
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.defaultView.innerHeight +
          content.document.defaultView.pageYOffset >=
        content.document.body.offsetHeight,
      "Scroll with Accel+ArrowDown"
    );
  });

  let closedPromise = BrowserTestUtils.waitForEvent(findBar, "findbarclose");
  await EventUtils.synthesizeKey("KEY_Escape");
  await closedPromise;

  info("Scrolling ok");

  BrowserTestUtils.removeTab(tab);
});

// This test loads an editable area within an iframe and then
// performs a search. Focusing the editable area should still
// allow keyboard events to be received.
add_task(async function test_hotkey_insubframe() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE_URI_WITHIFRAME
  );

  await gFindBarPromise;
  let findBar = gFindBar;

  // Focus the editable area within the frame.
  let browser = gBrowser.selectedBrowser;
  let frameBC = browser.browsingContext.children[0];
  await SpecialPowers.spawn(frameBC, [], async () => {
    content.document.body.focus();
    content.document.defaultView.focus();
  });

  // Start a find and wait for the findbar to open.
  let findBarOpenPromise = BrowserTestUtils.waitForEvent(
    gBrowser,
    "findbaropen"
  );
  EventUtils.synthesizeKey("f", { accelKey: true });
  await findBarOpenPromise;

  // Opening the findbar would have focused the find textbox.
  // Focus the editable area again.
  let cursorPos = await SpecialPowers.spawn(frameBC, [], async () => {
    content.document.body.focus();
    content.document.defaultView.focus();
    return content.getSelection().anchorOffset;
  });
  is(cursorPos, 0, "initial cursor position");

  // Try moving the caret.
  await BrowserTestUtils.synthesizeKey("KEY_ArrowRight", {}, frameBC);

  cursorPos = await SpecialPowers.spawn(frameBC, [], async () => {
    return content.getSelection().anchorOffset;
  });
  is(cursorPos, 1, "cursor moved");

  await closeFindbarAndWait(findBar);
  gBrowser.removeTab(tab);
});

/**
 * Reloading a page should use the same match case / whole word
 * state for the search.
 */
add_task(async function test_preservestate_on_reload() {
  for (let stateChange of ["case-sensitive", "entire-word"]) {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "data:text/html,<!DOCTYPE html><p>There is a cat named Theo in the kitchen with another cat named Catherine. The two of them are thirsty."
    );

    // Start a find and wait for the findbar to open.
    let findBarOpenPromise = BrowserTestUtils.waitForEvent(
      gBrowser,
      "findbaropen"
    );
    EventUtils.synthesizeKey("f", { accelKey: true });
    await findBarOpenPromise;

    let isEntireWord = stateChange == "entire-word";

    let findbar = await gBrowser.getFindBar();

    // Find some text.
    let promiseMatches = promiseGetMatchCount(findbar);
    await promiseFindFinished(gBrowser, "The", true);

    let matches = await promiseMatches;
    is(matches.current, 1, "Correct match position " + stateChange);
    is(matches.total, 7, "Correct number of matches " + stateChange);

    // Turn on the case sensitive or entire word option.
    findbar.getElement("find-" + stateChange).click();

    promiseMatches = promiseGetMatchCount(findbar);
    gFindBar.onFindAgainCommand();
    matches = await promiseMatches;
    is(
      matches.current,
      2,
      "Correct match position after state change matches " + stateChange
    );
    is(
      matches.total,
      isEntireWord ? 2 : 3,
      "Correct number after state change matches " + stateChange
    );

    // Reload the page.
    let loadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      true
    );
    gBrowser.reload();
    await loadedPromise;

    // Perform a find again. The state should be preserved.
    promiseMatches = promiseGetMatchCount(findbar);
    gFindBar.onFindAgainCommand();
    matches = await promiseMatches;
    is(
      matches.current,
      1,
      "Correct match position after reload and find again " + stateChange
    );
    is(
      matches.total,
      isEntireWord ? 2 : 3,
      "Correct number of matches after reload and find again " + stateChange
    );

    // Turn off the case sensitive or entire word option and find again.
    findbar.getElement("find-" + stateChange).click();

    promiseMatches = promiseGetMatchCount(findbar);
    gFindBar.onFindAgainCommand();
    matches = await promiseMatches;

    is(
      matches.current,
      isEntireWord ? 4 : 2,
      "Correct match position after reload and find again reset " + stateChange
    );
    is(
      matches.total,
      7,
      "Correct number of matches after reload and find again reset " +
        stateChange
    );

    findbar.clear();
    await closeFindbarAndWait(findbar);

    gBrowser.removeTab(tab);
  }
});

function promiseGetMatchCount(findbar) {
  return new Promise(resolve => {
    let resultListener = {
      onFindResult() {},
      onCurrentSelection() {},
      onHighlightFinished() {},
      onMatchesCountResult(response) {
        if (response.total > 0) {
          findbar.browser.finder.removeResultListener(resultListener);
          resolve(response);
        }
      },
    };
    findbar.browser.finder.addResultListener(resultListener);
  });
}

function promiseRemotenessChange(tab, shouldBeRemote) {
  return new Promise(resolve => {
    let browser = gBrowser.getBrowserForTab(tab);
    tab.addEventListener(
      "TabRemotenessChange",
      function () {
        resolve();
      },
      { once: true }
    );
    let remoteType = shouldBeRemote
      ? E10SUtils.DEFAULT_REMOTE_TYPE
      : E10SUtils.NOT_REMOTE;
    gBrowser.updateBrowserRemoteness(browser, { remoteType });
  });
}
