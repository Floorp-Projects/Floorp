/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.import("resource://gre/modules/PromiseUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm", this);

const WINDOW_TYPE = "navigator:view-source";

function openViewSourceWindow(aURI, aCallback) {
  let viewSourceWindow = openDialog("chrome://global/content/viewSource.xul", null, null, aURI);
  viewSourceWindow.addEventListener("pageshow", function pageShowHandler(event) {
    // Wait for the inner window to load, not viewSourceWindow.
    if (event.target.location == "view-source:" + aURI) {
      info("View source window opened: " + event.target.location);
      viewSourceWindow.removeEventListener("pageshow", pageShowHandler);
      aCallback(viewSourceWindow);
    }
  });
}

function loadViewSourceWindow(URL) {
  return new Promise((resolve) => {
    openViewSourceWindow(URL, resolve);
  })
}

function closeViewSourceWindow(aWindow, aCallback) {
  return new Promise(resolve => {
    Services.wm.addListener({
      onCloseWindow() {
        Services.wm.removeListener(this);
        if (aCallback) {
          executeSoon(aCallback);
        }
        resolve();
      }
    });
    aWindow.close();
  });
}

function testViewSourceWindow(aURI, aTestCallback, aCloseCallback) {
  openViewSourceWindow(aURI, function(aWindow) {
    aTestCallback(aWindow);
    closeViewSourceWindow(aWindow, aCloseCallback);
  });
}

function waitForViewSourceWindow() {
  return new Promise(resolve => {
    let windowListener = {
      onOpenWindow(xulWindow) {
        let win = xulWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindow);
        win.addEventListener("load", function() {
          if (win.document.documentElement.getAttribute("windowtype") !=
              WINDOW_TYPE) {
            return;
          }
          // Found the window
          resolve(win);
          Services.wm.removeListener(windowListener);
        }, {once: true});
      },
      onCloseWindow() {},
      onWindowTitleChange() {}
    };
    Services.wm.addListener(windowListener);
  });
}

/**
 * Opens view source for a browser.
 *
 * @param browser - the <xul:browser> to open view source for.
 * @returns the new tab or window which shows the source.
 */
function openViewSource(browser) {
  let openPromise;
  if (Services.prefs.getBoolPref("view_source.tab")) {
    openPromise = BrowserTestUtils.waitForNewTab(gBrowser, null);
  } else {
    openPromise = waitForViewSourceWindow();
  }

  window.BrowserViewSource(browser);

  return openPromise;
}

/**
 * Opens a view source tab / window for a selection (View Selection Source)
 * within the currently selected browser in gBrowser.
 *
 * @param aCSSSelector - used to specify a node within the selection to
 *                       view the source of. It is expected that this node is
 *                       within an existing selection.
 * @returns the new tab / window which shows the source.
 */
async function openViewPartialSource(aCSSSelector) {
  let contentAreaContextMenuPopup =
    document.getElementById("contentAreaContextMenu");
  let popupShownPromise =
    BrowserTestUtils.waitForEvent(contentAreaContextMenuPopup, "popupshown");
  await BrowserTestUtils.synthesizeMouseAtCenter(aCSSSelector,
          { type: "contextmenu", button: 2 }, gBrowser.selectedBrowser);
  await popupShownPromise;

  let openPromise;
  if (Services.prefs.getBoolPref("view_source.tab")) {
    openPromise = BrowserTestUtils.waitForNewTab(gBrowser, null);
  } else {
    openPromise = waitForViewSourceWindow();
  }

  let popupHiddenPromise =
    BrowserTestUtils.waitForEvent(contentAreaContextMenuPopup, "popuphidden");
  let item = document.getElementById("context-viewpartialsource-selection");
  EventUtils.synthesizeMouseAtCenter(item, {});
  await popupHiddenPromise;

  return openPromise;
}

/**
 * Opens a view source tab for a frame (View Frame Source) within the
 * currently selected browser in gBrowser.
 *
 * @param aCSSSelector - used to specify the frame to view the source of.
 * @returns the new tab which shows the source.
 */
async function openViewFrameSourceTab(aCSSSelector) {
  let contentAreaContextMenuPopup =
    document.getElementById("contentAreaContextMenu");
  let popupShownPromise =
    BrowserTestUtils.waitForEvent(contentAreaContextMenuPopup, "popupshown");
  await BrowserTestUtils.synthesizeMouseAtCenter(aCSSSelector,
          { type: "contextmenu", button: 2 }, gBrowser.selectedBrowser);
  await popupShownPromise;

  let frameContextMenu = document.getElementById("frame");
  popupShownPromise =
    BrowserTestUtils.waitForEvent(frameContextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(frameContextMenu, {});
  await popupShownPromise;

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null);

  let popupHiddenPromise =
    BrowserTestUtils.waitForEvent(frameContextMenu, "popuphidden");
  let item = document.getElementById("context-viewframesource");
  EventUtils.synthesizeMouseAtCenter(item, {});
  await popupHiddenPromise;

  return newTabPromise;
}

registerCleanupFunction(function() {
  var windows = Services.wm.getEnumerator(WINDOW_TYPE);
  ok(!windows.hasMoreElements(), "No remaining view source windows still open");
  while (windows.hasMoreElements())
    windows.getNext().close();
});

/**
 * For a given view source tab / window, wait for the source loading step to
 * complete.
 */
function waitForSourceLoaded(tabOrWindow) {
  return new Promise(resolve => {
    let mm = tabOrWindow.messageManager ||
             tabOrWindow.linkedBrowser.messageManager;
    mm.addMessageListener("ViewSource:SourceLoaded", function sourceLoaded() {
      mm.removeMessageListener("ViewSource:SourceLoaded", sourceLoaded);
      setTimeout(resolve, 0);
    });
  });
}

/**
 * Open a new document in a new tab, select part of it, and view the source of
 * that selection. The document is not closed afterwards.
 *
 * @param aURI - url to load
 * @param aCSSSelector - used to specify a node to select. All of this node's
 *                       children will be selected.
 * @returns the new tab / window which shows the source.
 */
async function openDocumentSelect(aURI, aCSSSelector) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, aURI);
  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
  });

  await ContentTask.spawn(gBrowser.selectedBrowser, { selector: aCSSSelector }, async function(arg) {
    let element = content.document.querySelector(arg.selector);
    content.getSelection().selectAllChildren(element);
  });

  let tabOrWindow = await openViewPartialSource(aCSSSelector);

  // Wait until the source has been loaded.
  await waitForSourceLoaded(tabOrWindow);

  return tabOrWindow;
}

function pushPrefs(...aPrefs) {
  return SpecialPowers.pushPrefEnv({"set": aPrefs});
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
