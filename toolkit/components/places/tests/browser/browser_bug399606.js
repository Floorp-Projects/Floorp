/* ***** BEGIN LICENSE BLOCK *****
 *   Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *   Marco Bonardo <mak77@bonardo.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

gBrowser.selectedTab = gBrowser.addTab();

function test() {
  waitForExplicitFinish();

  var URIs = [
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-window.location.href.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-history.go-0.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-location.replace.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-location.reload.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-httprefresh.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-window.location.html",
  ];
  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);

  // Create and add history observer.
  var historyObserver = {
    visitCount: Array(),
    onBeginUpdateBatch: function () {},
    onEndUpdateBatch: function () {},
    onVisit: function (aURI, aVisitID, aTime, aSessionID, aReferringID,
                      aTransitionType) {
      info("Received onVisit: " + aURI.spec);
      if (aURI.spec in this.visitCount)
        this.visitCount[aURI.spec]++;
      else
        this.visitCount[aURI.spec] = 1;
    },
    onTitleChanged: function () {},
    onBeforeDeleteURI: function () {},
    onDeleteURI: function () {},
    onClearHistory: function () {},
    onPageChanged: function () {},
    onDeleteVisits: function () {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
  };
  hs.addObserver(historyObserver, false);

  function confirm_results() {
    gBrowser.removeCurrentTab();
    hs.removeObserver(historyObserver, false);
    for (let aURI in historyObserver.visitCount) {
      is(historyObserver.visitCount[aURI], 1,
         "onVisit has been received right number of times for " + aURI);
    }
    waitForClearHistory(finish);
  }

  var loadCount = 0;
  function handleLoad(aEvent) {
    loadCount++;
    info("new load count is " + loadCount);

    if (loadCount == 3) {
      gBrowser.removeEventListener("DOMContentLoaded", handleLoad, true);
      content.location.href = "about:blank";
      executeSoon(check_next_uri);
    }
  }

  function check_next_uri() {
    if (URIs.length) {
      let uri = URIs.shift();
      loadCount = 0;
      gBrowser.addEventListener("DOMContentLoaded", handleLoad, true);
      content.location.href = uri;
    }
    else {
      confirm_results();
    }
  }
  executeSoon(check_next_uri);
}
