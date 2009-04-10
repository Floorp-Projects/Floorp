/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Dietrich Ayala <dietrich@mozilla.com>
 *  Dan Mills <thunder@mozilla.com>
 *  Seth Spitzer <sspitzer@mozilla.com>
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

// AddURI transactions are created lazily.
do_test_pending();

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

var ghist = Cc["@mozilla.org/browser/global-history;2"].getService(Ci.nsIGlobalHistory2);

// create and add history observer
var observer = {
  onBeginUpdateBatch: function() {},
  onEndUpdateBatch: function() {},
  visitCount: 0,
  onVisit: function(aURI, aVisitID, aTime, aSessionID, aReferringID, aTransitionType) {
    this.visitCount++;
    dump("onVisit: " + aURI.spec + "\n");
    confirm_results();
  },
  onTitleChanged: function(aURI, aPageTitle) {},
  onBeforeDeleteURI: function(aURI) {},
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

// main
function run_test() {
  var now = Date.now();
  var testURI = uri("http://fez.com");
  ghist.addURI(testURI, false, true, null);
  ghist.addURI(testURI, false, true, testURI);
  // lazy message timer is 3000, see LAZY_MESSAGE_TIMEOUT
  do_timeout(3500, "confirm_results();");
}

function confirm_results() {
  var options = histsvc.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_VISIT;
  options.includeHidden = true;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  do_check_eq(cc, 1);
  root.containerOpen = false;
  do_test_finished();
}
