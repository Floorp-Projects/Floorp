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

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

/**
 * Checks to see that a URI is in the database.
 *
 * @param aURI
 *        The URI to check.
 * @returns true if the URI is in the DB, false otherwise.
 */
function uri_in_db(aURI) {
  var options = histsvc.getNewQueryOptions();
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_URI;
  options.includeHidden = true;
  var query = histsvc.getNewQuery();
  query.uri = aURI;
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  return (root.childCount == 1);
}

// main
function run_test() {
  // Test that when we get nsIDownloadHistory that it's the one places
  // implements.
  var dh = Cc["@mozilla.org/browser/download-history;1"].
           getService(Ci.nsIDownloadHistory);
  do_check_true(dh instanceof Ci.nsINavHistoryService);

  // now to make sure all the functionality works!
  const NS_LINK_VISITED_EVENT_TOPIC = "link-visited";
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var testURI = ios.newURI("http://google.com/", null, null);

  do_check_false(uri_in_db(testURI));

  var topicReceived = false;
  var obs = {
    observe: function tlvo_observe(aSubject, aTopic, aData)
    {
      if (NS_LINK_VISITED_EVENT_TOPIC == aTopic) {
        do_check_eq(testURI, aSubject);
        topicReceived = true;
      }
    }
  };

  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver(obs, NS_LINK_VISITED_EVENT_TOPIC, false);

  var referrerURI = ios.newURI("http://yahoo.com", null, null);
  do_check_false(uri_in_db(referrerURI));
  do_check_false(uri_in_db(testURI));
  dh.addDownload(testURI, referrerURI, Date.now() * 1000);
  do_check_true(topicReceived);
  do_check_true(uri_in_db(testURI));
  do_check_true(uri_in_db(referrerURI));
}
