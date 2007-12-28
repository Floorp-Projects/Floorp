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
 *  Seth Spitzer <sspitzer@mozilla.org>
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

// Get global history service
try {
  var gh = Cc["@mozilla.org/browser/global-history;2"].getService(Ci.nsIBrowserHistory);
} catch(ex) {
  do_throw("Could not get global history service\n");
} 

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

function add_uri_to_history(aURI) {
  gh.addURI(aURI,
            false, // not redirect
            true, // top level
            null); // no referrer, so that we'll use the markPageAs hint
}

var gVisits = [{url: "http://www.mozilla.com/",
                transition: histsvc.TRANSITION_TYPED},
               {url: "http://www.google.com/", 
                transition: histsvc.TRANSITION_BOOKMARK},
               {url: "http://www.espn.com/",
                transition: histsvc.TRANSITION_LINK}];

// main
function run_test() {
  for each (var visit in gVisits) {
    if (visit.transition == histsvc.TRANSITION_TYPED)
      gh.markPageAsTyped(uri(visit.url));
    else if (visit.transition == histsvc.TRANSITION_BOOKMARK)
      gh.markPageAsFollowedBookmark(uri(visit.url))
    else {
     // because it is a top level visit with no referrer,
     // it will result in TRANSITION_LINK
    }
    add_uri_to_history(uri(visit.url));
  }

  do_test_pending();
}

// create and add history observer
var observer = {
  _visitCount: 0,
  onBeginUpdateBatch: function() {
  },
  onEndUpdateBatch: function() {
  },
  onVisit: function(aURI, aVisitID, aTime, aSessionID, aReferringID, aTransitionType) {
    do_check_eq(aURI.spec, gVisits[this._visitCount].url);
    do_check_eq(aTransitionType, gVisits[this._visitCount].transition);
    this._visitCount++;

    if (this._visitCount == gVisits.length)
      do_test_finished();
  },
  onTitleChanged: function(aURI, aPageTitle) {
  },
  onDeleteURI: function(aURI) {
  },
  onClearHistory: function() {
  },
  onPageChanged: function(aURI, aWhat, aValue) {
  },
  onPageExpired: function(aURI, aVisitTime, aWholeEntry) {
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavBookmarkObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

histsvc.addObserver(observer, false);
