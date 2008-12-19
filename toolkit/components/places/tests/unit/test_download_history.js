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
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

// Get services
var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
              getService(Ci.nsINavHistoryService);
let bh = histsvc.QueryInterface(Ci.nsIBrowserHistory);
var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch);
var dh = Cc["@mozilla.org/browser/download-history;1"].
         getService(Ci.nsIDownloadHistory);
// Test that this nsIDownloadHistory is the one places implements.
do_check_true(dh instanceof Ci.nsINavHistoryService);
var pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);


const NS_LINK_VISITED_EVENT_TOPIC = "link-visited";
const DISABLE_HISTORY_PREF = "browser.history_expire_days";
const PB_KEEP_SESSION_PREF = "browser.privatebrowsing.keep_current_session";

var testURI = uri("http://google.com/");
var referrerURI = uri("http://yahoo.com");

/**
 * Checks to see that a URI is in the database.
 *
 * @param aURI
 *        The URI to check.
 * @param aExpected
 *        Boolean result expected from the db lookup.
 */
function uri_in_db(aURI, aExpected) {
  var options = histsvc.getNewQueryOptions();
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_URI;
  options.includeHidden = true;
  var query = histsvc.getNewQuery();
  query.uri = aURI;
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  root.containerOpen = false;
  var checker = aExpected ? do_check_true : do_check_false;
  checker(cc == 1);
}

function test_dh() {
  dh.addDownload(testURI, referrerURI, Date.now() * 1000);

  do_check_true(observer.topicReceived);
  uri_in_db(testURI, true);
  uri_in_db(referrerURI, true);
}

function test_dh_privateBrowsing() {
  prefs.setBoolPref(PB_KEEP_SESSION_PREF, true);
  pb.privateBrowsingEnabled = true;

  dh.addDownload(testURI, referrerURI, Date.now() * 1000);

  do_check_false(observer.topicReceived);
  uri_in_db(testURI, false);
  uri_in_db(referrerURI, false);

  // Cleanup
  pb.privateBrowsingEnabled = false;
}

function test_dh_disabledHistory() {
  // Disable history
  prefs.setIntPref(DISABLE_HISTORY_PREF, 0);

  dh.addDownload(testURI, referrerURI, Date.now() * 1000);

  do_check_false(observer.topicReceived);
  uri_in_db(testURI, false);
  uri_in_db(referrerURI, false);

  // Cleanup
  prefs.setIntPref(DISABLE_HISTORY_PREF, 180);
}

var tests = [
  test_dh,
  test_dh_privateBrowsing,
  test_dh_disabledHistory,
];

var observer = {
  topicReceived: false,
  observe: function tlvo_observe(aSubject, aTopic, aData)
  {
    if (NS_LINK_VISITED_EVENT_TOPIC == aTopic) {
      this.topicReceived = true;
    }
  }
};
os.addObserver(observer, NS_LINK_VISITED_EVENT_TOPIC, false);

// main
function run_test() {
  while (tests.length) {
    // Sanity checks
    uri_in_db(testURI, false);
    uri_in_db(referrerURI, false);

    (tests.shift())();

    // Cleanup
    bh.removeAllPages();
    observer.topicReceived = false;
  }

  os.removeObserver(observer, NS_LINK_VISITED_EVENT_TOPIC);
}
