/* eslint-disable mozilla/no-arbitrary-setTimeout */
ChromeUtils.import("resource://gre/modules/Timer.jsm", this);

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_preservestate_on_reload() {
  var oldAction = changeMimeHandler(Ci.nsIHandlerInfo.useSystemDefault, true);
  let dirFileObj = getChromeDir(getResolvedURI(gTestPath));
  dirFileObj.append("file_pdfjs_test.pdf");

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    dirFileObj.path
  );

  // Start a find and wait for the findbar to open.
  let findBarOpenPromise = BrowserTestUtils.waitForEvent(
    gBrowser,
    "findbaropen"
  );
  EventUtils.synthesizeKey("f", { accelKey: true });

  await findBarOpenPromise;

  let findbar = await gBrowser.getFindBar();

  // Find some text.
  await promiseFindFinished("The", true);

  // We can't access PdfjsParent directly
  is(
    findbar._foundMatches.value.includes("86"),
    true,
    "Less than 86 instances were found"
  );

  findbar.clear();
  await closeFindbarAndWait(findbar);
  changeMimeHandler(oldAction[0], oldAction[1]);

  gBrowser.removeTab(tab);
});

async function closeFindbarAndWait() {
  let awaitTransitionEnd = BrowserTestUtils.waitForEvent(
    gFindBar,
    "transitionend",
    false,
    event => {
      return event.propertyName == "visibility";
    }
  );
  gFindBar.close();
  await awaitTransitionEnd;
}

async function promiseFindFinished(searchText, highlightOn) {
  let findbar = await gBrowser.getFindBar();
  findbar.startFind(findbar.FIND_NORMAL);
  return new Promise(resolve => {
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
        if (aData === null) {
          info("Result listener not called, timeout reached.");
        }
        clearTimeout(findTimeout);
        // below is mull
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
