/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * One-time DOMContentLoaded callback.
 */
function waitForLoad(callback)
{
  gTab.linkedBrowser.addEventListener("load", function()
  {
    gTab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    callback();
  }, true);
}

let gTab;
function test()
{
  if (!("@mozilla.org/privatebrowsing;1" in Cc)) {
    todo(false, "PB service is not available, bail out");
    return;
  }

  gTab = gBrowser.selectedTab = gBrowser.addTab();

  waitForExplicitFinish();

  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  pb.privateBrowsingEnabled = true;

  const pageURI ="http://example.org/tests/toolkit/components/places/tests/browser/favicon.html";
  content.location.href = pageURI;
  waitForLoad(function()
  {
    PlacesUtils.favicons.getFaviconURLForPage(
      NetUtil.newURI(pageURI),
      function(uri, dataLen, data, mimeType) {
        is(uri, null, "no result should be found");
        gBrowser.removeCurrentTab();
        pb.privateBrowsingEnabled = false;
        Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
        finish();
      });
  });
}

