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

function test()
{
  const TEST_URI = "http://example.com/tests/toolkit/components/places/tests/browser/399606-history.go-0.html";

  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);

  // create and add history observer
  var observer = {
    onBeginUpdateBatch: function() {},
    onEndUpdateBatch: function() {},
    onVisit: function(aURI, aVisitID, aTime, aSessionID, aReferringID,
                      aTransitionType) {
      dump("onVisit: " + aURI.spec + "\n");
      confirm_results();

      histsvc.removeObserver(observer, false);
      win.content.document.location.href = "about:blank";
      finish();
    },
    onTitleChanged: function(aURI, aPageTitle) {},
    onDeleteURI: function(aURI) {},
    onClearHistory: function() {},
    onPageChanged: function(aURI, aWhat, aValue) {},
    onPageExpired: function(aURI, aVisitTime, aWholeEntry) {},
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsINavHistoryObserver) ||
          iid.equals(Ci.nsISupports)) {
        return this;
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  };
  
  histsvc.addObserver(observer, false);
  
  // If LAZY_ADD is ever disabled, this function will not be correct
  var loadCount = 0;
  function confirm_results() {
    var options = histsvc.getNewQueryOptions();
    options.resultType = options.RESULTS_AS_VISIT;
    options.includeHidden = true;
    var query = histsvc.getNewQuery();
    var uri = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService).
              newURI(TEST_URI, null, null);
    dump("query uri is " + uri.spec + "\n");
    query.uri = uri;
    var result = histsvc.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    var cc = root.childCount;
    // TODO bug 415004
    todo_is(cc, 1, "Visit count is what we expect");
    ok(loadCount > 1, "Load count is greater than 1");
    root.containerOpen = false;
  }

  var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
           getService(Ci.nsIWindowMediator);
  var win = wm.getMostRecentWindow("navigator:browser");

  function handleLoad(aEvent) {
    dump("location is " + aEvent.originalTarget.location.href + "\n");
    loadCount++;
    dump("new load count is " + loadCount + "\n");

    if (loadCount == 3)
      win.getBrowser().removeEventListener("DOMContentLoaded", handleLoad, false);
  }

  win.getBrowser().addEventListener("DOMContentLoaded", handleLoad, false);

  // load page
  win.content.document.location.href = TEST_URI;

  // let our load handler handle the rest of the test
  waitForExplicitFinish();
}
