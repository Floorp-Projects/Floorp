/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function openViewSourceWindow(aURI, aCallback) {
  let viewSourceWindow = openDialog("chrome://global/content/viewSource.xul", null, null, aURI);
  viewSourceWindow.addEventListener("pageshow", function pageShowHandler(event) {
    // Wait for the inner window to load, not viewSourceWindow.
    if (event.target.location == "view-source:" + aURI) {
      info("View source window opened: " + event.target.location);
      viewSourceWindow.removeEventListener("pageshow", pageShowHandler, false);
      aCallback(viewSourceWindow);
    }
  }, false);
}

function closeViewSourceWindow(aWindow, aCallback) {
  Services.wm.addListener({
    onCloseWindow: function() {
      Services.wm.removeListener(this);
      executeSoon(aCallback);
    }
  });
  aWindow.close();
}

function testViewSourceWindow(aURI, aTestCallback, aCloseCallback) {
  openViewSourceWindow(aURI, function(aWindow) {
    aTestCallback(aWindow);
    closeViewSourceWindow(aWindow, aCloseCallback);
  });
}

function openViewPartialSourceWindow(aReference, aCallback) {
  let viewSourceWindow = openDialog("chrome://global/content/viewPartialSource.xul",
                                    null, null, null, null, aReference, "selection");
  viewSourceWindow.addEventListener("pageshow", function pageShowHandler(event) {
    // Wait for the inner window to load, not viewSourceWindow.
    if (/^view-source:/.test(event.target.location)) {
      info("View source window opened: " + event.target.location);
      viewSourceWindow.removeEventListener("pageshow", pageShowHandler, false);
      aCallback(viewSourceWindow);
    }
  }, false);
}

registerCleanupFunction(function() {
  var windows = Services.wm.getEnumerator("navigator:view-source");
  ok(!windows.hasMoreElements(), "No remaining view source windows still open");
  while (windows.hasMoreElements())
    windows.getNext().close();
});

function openDocument(aURI, aCallback) {
  let tab = gBrowser.addTab(aURI);
  let browser = tab.linkedBrowser;
  browser.addEventListener("DOMContentLoaded", function pageLoadedListener() {
    browser.removeEventListener("DOMContentLoaded", pageLoadedListener, false);
    aCallback(tab);
  }, false);
  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
  });
}

function openDocumentSelect(aURI, aCSSSelector, aCallback) {
  openDocument(aURI, function(aTab) {
    let element = aTab.linkedBrowser.contentDocument.querySelector(aCSSSelector);
    let selection = aTab.linkedBrowser.contentWindow.getSelection();
    selection.selectAllChildren(element);

    openViewPartialSourceWindow(selection, aCallback);
  });
}
