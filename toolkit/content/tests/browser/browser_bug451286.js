Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/WindowSnapshot.js",
  this
);

add_task(async function() {
  const SEARCH_TEXT = "text";
  const DATAURI = "data:text/html," + SEARCH_TEXT;

  // Bug 451286. An iframe that should be highlighted
  let visible = "<iframe id='visible' src='" + DATAURI + "'></iframe>";

  // Bug 493658. An invisible iframe that shouldn't interfere with
  // highlighting matches lying after it in the document
  let invisible =
    "<iframe id='invisible' style='display: none;' " +
    "src='" +
    DATAURI +
    "'></iframe>";

  let uri = DATAURI + invisible + SEARCH_TEXT + visible + SEARCH_TEXT;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri);
  let contentRect = tab.linkedBrowser.getBoundingClientRect();
  let noHighlightSnapshot = snapshotRect(window, contentRect);
  ok(noHighlightSnapshot, "Got noHighlightSnapshot");

  await openFindBarAndWait();
  gFindBar._findField.value = SEARCH_TEXT;
  await findAgainAndWait();
  var matchCase = gFindBar.getElement("find-case-sensitive");
  if (matchCase.checked) {
    matchCase.doCommand();
  }

  // Turn on highlighting
  await toggleHighlightAndWait(true);
  await closeFindBarAndWait();

  // Take snapshot of highlighting
  let findSnapshot = snapshotRect(window, contentRect);
  ok(findSnapshot, "Got findSnapshot");

  // Now, remove the highlighting, and take a snapshot to compare
  // to our original state
  await openFindBarAndWait();
  await toggleHighlightAndWait(false);
  await closeFindBarAndWait();

  let unhighlightSnapshot = snapshotRect(window, contentRect);
  ok(unhighlightSnapshot, "Got unhighlightSnapshot");

  // Select the matches that should have been highlighted manually
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    let doc = content.document;
    let win = doc.defaultView;

    // Create a manual highlight in the visible iframe to test bug 451286
    let iframe = doc.getElementById("visible");
    let ifBody = iframe.contentDocument.body;
    let range = iframe.contentDocument.createRange();
    range.selectNodeContents(ifBody.childNodes[0]);
    let ifWindow = iframe.contentWindow;
    let ifDocShell = ifWindow.docShell;

    let ifController = ifDocShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsISelectionDisplay)
      .QueryInterface(Ci.nsISelectionController);

    let frameFindSelection = ifController.getSelection(
      ifController.SELECTION_FIND
    );
    frameFindSelection.addRange(range);

    // Create manual highlights in the main document (the matches that lie
    // before/after the iframes
    let docShell = win.docShell;

    let controller = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsISelectionDisplay)
      .QueryInterface(Ci.nsISelectionController);

    let docFindSelection = controller.getSelection(ifController.SELECTION_FIND);

    range = doc.createRange();
    range.selectNodeContents(doc.body.childNodes[0]);
    docFindSelection.addRange(range);
    range = doc.createRange();
    range.selectNodeContents(doc.body.childNodes[2]);
    docFindSelection.addRange(range);
    range = doc.createRange();
    range.selectNodeContents(doc.body.childNodes[4]);
    docFindSelection.addRange(range);
  });

  // Take snapshot of manual highlighting
  let manualSnapshot = snapshotRect(window, contentRect);
  ok(manualSnapshot, "Got manualSnapshot");

  // Test 1: Were the matches in iframe correctly highlighted?
  let res = compareSnapshots(findSnapshot, manualSnapshot, true);
  ok(res[0], "Matches found in iframe correctly highlighted");

  // Test 2: Were the matches in iframe correctly unhighlighted?
  res = compareSnapshots(noHighlightSnapshot, unhighlightSnapshot, true);
  ok(res[0], "Highlighting in iframe correctly removed");

  BrowserTestUtils.removeTab(tab);
});

function toggleHighlightAndWait(shouldHighlight) {
  return new Promise(resolve => {
    let listener = {
      onFindResult() {},
      onHighlightFinished() {
        gFindBar.browser.finder.removeResultListener(listener);
        resolve();
      },
      onMatchesCountResult() {},
    };
    gFindBar.browser.finder.addResultListener(listener);
    gFindBar.toggleHighlight(shouldHighlight);
  });
}

function findAgainAndWait() {
  return new Promise(resolve => {
    let listener = {
      onFindResult() {
        gFindBar.browser.finder.removeResultListener(listener);
        resolve();
      },
      onHighlightFinished() {},
      onMatchesCountResult() {},
    };
    gFindBar.browser.finder.addResultListener(listener);
    gFindBar.onFindAgainCommand();
  });
}

async function openFindBarAndWait() {
  await gFindBarPromise;
  let awaitTransitionEnd = BrowserTestUtils.waitForEvent(
    gFindBar,
    "transitionend"
  );
  gFindBar.open();
  await awaitTransitionEnd;
}

// This test is comparing snapshots. It is necessary to wait for the gFindBar
// to close before taking the snapshot so the gFindBar does not take up space
// on the new snapshot.
async function closeFindBarAndWait() {
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
