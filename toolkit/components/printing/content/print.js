/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { gBrowser, PrintUtils } = window.docShell.chromeEventHandler.ownerGlobal;

function initialize(sourceBrowser) {
  document.getElementById("tab-title").textContent = sourceBrowser.contentTitle;
  document.getElementById("open-dialog-link").addEventListener("click", e => {
    PrintUtils.printWindow(sourceBrowser.browsingContext);
  });
}

function getSourceBrowser() {
  let params = new URLSearchParams(location.search);
  let browsingContextId = params.get("browsingContextId");
  if (!browsingContextId) {
    return null;
  }
  let browsingContext = BrowsingContext.get(browsingContextId);
  if (!browsingContext) {
    return null;
  }
  return browsingContext.embedderElement;
}

document.addEventListener("DOMContentLoaded", e => {
  let sourceBrowser = getSourceBrowser();
  if (sourceBrowser) {
    initialize(sourceBrowser);
  } else {
    console.error("No source browser");
  }
});
