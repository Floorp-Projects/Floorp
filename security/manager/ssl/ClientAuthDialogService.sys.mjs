/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ClientAuthDialogService implements nsIClientAuthDialogService, and aims to
// open a dialog asking the user to select a client authentication certificate.
// Ideally the dialog will be tab-modal to the tab corresponding to the load
// that resulted in the request for the client authentication certificate.
export function ClientAuthDialogService() {}

// Given a loadContext (CanonicalBrowsingContext), attempts to return a
// TabDialogBox for the browser corresponding to loadContext.
function getTabDialogBoxForLoadContext(loadContext) {
  let tabBrowser = loadContext?.topFrameElement?.getTabBrowser();
  if (!tabBrowser) {
    return null;
  }
  for (let browser of tabBrowser.browsers) {
    if (browser.browserId == loadContext.top?.browserId) {
      return tabBrowser.getTabDialogBox(browser);
    }
  }
  return null;
}

ClientAuthDialogService.prototype = {
  classID: Components.ID("{d7d2490d-2640-411b-9f09-a538803c11ee}"),
  QueryInterface: ChromeUtils.generateQI(["nsIClientAuthDialogService"]),

  chooseCertificate: function ClientAuthDialogService_chooseCertificate(
    hostname,
    certArray,
    loadContext,
    callback
  ) {
    const clientAuthAskURI = "chrome://pippki/content/clientauthask.xhtml";
    let retVals = { cert: null, rememberDecision: false };
    // First attempt to find a TabDialogBox for the loadContext. This allows
    // for a tab-modal dialog specific to the tab causing the load, which is a
    // better user experience.
    let tabDialogBox = getTabDialogBoxForLoadContext(loadContext);
    if (tabDialogBox) {
      tabDialogBox
        .open(clientAuthAskURI, {}, { hostname, certArray, retVals })
        .closedPromise.then(() => {
          callback.certificateChosen(retVals.cert, retVals.rememberDecision);
        });
      return;
    }
    // Otherwise, attempt to open a window-modal dialog on the window that at
    // least has the tab the load is occurring in.
    let browserWindow = loadContext?.topFrameElement?.ownerGlobal;
    // Failing that, open a window-modal dialog on the most recent window.
    if (!browserWindow) {
      browserWindow = Services.wm.getMostRecentBrowserWindow();
    }
    if (browserWindow) {
      browserWindow.gDialogBox
        .open(clientAuthAskURI, { hostname, certArray, retVals })
        .then(() => {
          callback.certificateChosen(retVals.cert, retVals.rememberDecision);
        });
      return;
    }
    // Otherwise, continue the connection with no certificate.
    callback.certificateChosen(null, false);
  },
};
