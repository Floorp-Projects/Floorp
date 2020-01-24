/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);

/**
 * Wait for view source tab after calling given function to open it.
 *
 * @param open - a function to open view source.
 * @returns the new tab which shows the source.
 */
async function waitForViewSourceTab(open) {
  let sourceLoadedPromise;
  let tabPromise;

  tabPromise = new Promise(resolve => {
    gBrowser.tabContainer.addEventListener(
      "TabOpen",
      event => {
        let tab = event.target;
        sourceLoadedPromise = waitForSourceLoaded(tab);
        resolve(tab);
      },
      { once: true }
    );
  });

  await open();

  let tab = await tabPromise;
  await sourceLoadedPromise;
  return tab;
}

/**
 * Opens view source for a browser.
 *
 * @param browser - the <xul:browser> to open view source for.
 * @returns the new tab which shows the source.
 */
function openViewSourceForBrowser(browser) {
  return waitForViewSourceTab(() => {
    window.BrowserViewSource(browser);
  });
}

/**
 * Opens a view source tab. (View Source)
 * within the currently selected browser in gBrowser.
 *
 * @returns the new tab which shows the source.
 */
async function openViewSource() {
  let contentAreaContextMenuPopup = document.getElementById(
    "contentAreaContextMenu"
  );
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenuPopup,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
  await popupShownPromise;

  return waitForViewSourceTab(async () => {
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      contentAreaContextMenuPopup,
      "popuphidden"
    );
    let item = document.getElementById("context-viewsource");
    EventUtils.synthesizeMouseAtCenter(item, {});
    await popupHiddenPromise;
  });
}

/**
 * Opens a view source tab for a selection (View Selection Source)
 * within the currently selected browser in gBrowser.
 *
 * @param aCSSSelector - used to specify a node within the selection to
 *                       view the source of. It is expected that this node is
 *                       within an existing selection.
 * @param aBrowsingContext - browsing context containing a subframe (optional).
 * @returns the new tab which shows the source.
 */
async function openViewPartialSource(
  aCSSSelector,
  aBrowsingContext = gBrowser.selectedBrowser
) {
  let contentAreaContextMenuPopup = document.getElementById(
    "contentAreaContextMenu"
  );
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenuPopup,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    aCSSSelector,
    { type: "contextmenu", button: 2 },
    aBrowsingContext
  );
  await popupShownPromise;

  return waitForViewSourceTab(async () => {
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      contentAreaContextMenuPopup,
      "popuphidden"
    );
    let item = document.getElementById("context-viewpartialsource-selection");
    EventUtils.synthesizeMouseAtCenter(item, {});
    await popupHiddenPromise;
  });
}

/**
 * Opens a view source tab for a frame (View Frame Source) within the
 * currently selected browser in gBrowser.
 *
 * @param aCSSSelector - used to specify the frame to view the source of.
 * @returns the new tab which shows the source.
 */
async function openViewFrameSourceTab(aCSSSelector) {
  let contentAreaContextMenuPopup = document.getElementById(
    "contentAreaContextMenu"
  );
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenuPopup,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    aCSSSelector,
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
  await popupShownPromise;

  let frameContextMenu = document.getElementById("frame");
  popupShownPromise = BrowserTestUtils.waitForEvent(
    frameContextMenu,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(frameContextMenu, {});
  await popupShownPromise;

  return waitForViewSourceTab(async () => {
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      frameContextMenu,
      "popuphidden"
    );
    let item = document.getElementById("context-viewframesource");
    EventUtils.synthesizeMouseAtCenter(item, {});
    await popupHiddenPromise;
  });
}

/**
 * For a given view source tab, wait for the source loading step to
 * complete.
 */
function waitForSourceLoaded(tab) {
  return BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "pageshow",
    false,
    event => String(event.target.location).startsWith("view-source")
  );
}

/**
 * Open a new document in a new tab, select part of it, and view the source of
 * that selection. The document is not closed afterwards.
 *
 * @param aURI - url to load
 * @param aCSSSelector - used to specify a node to select. All of this node's
 *                       children will be selected.
 * @returns the new tab which shows the source.
 */
async function openDocumentSelect(aURI, aCSSSelector) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, aURI);
  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
  });

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ selector: aCSSSelector }],
    async function(arg) {
      let element = content.document.querySelector(arg.selector);
      content.getSelection().selectAllChildren(element);
    }
  );

  return openViewPartialSource(aCSSSelector);
}

/**
 * Open a new document in a new tab and view the source of whole page.
 * The document is not closed afterwards.
 *
 * @param aURI - url to load
 * @returns the new tab which shows the source.
 */
async function openDocument(aURI) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, aURI);
  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
  });

  return openViewSource();
}

function pushPrefs(...aPrefs) {
  return SpecialPowers.pushPrefEnv({ set: aPrefs });
}

function waitForPrefChange(pref) {
  let deferred = PromiseUtils.defer();
  let observer = () => {
    Preferences.ignore(pref, observer);
    deferred.resolve();
  };
  Preferences.observe(pref, observer);
  return deferred.promise;
}
