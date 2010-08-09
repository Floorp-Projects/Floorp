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
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Dahl <ddahl@mozilla.com>
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

/*
 * Need to test that removing a page from autocomplete actually removes a page
 * Description From  Shawn Wilsher :sdwilsh   2009-02-18 11:29:06 PST
 * We don't test the code path of onValueRemoved
 * for the autocomplete implementation
 * Bug 479089
 */

var ios = Cc["@mozilla.org/network/io-service;1"].
getService(Components.interfaces.nsIIOService);

var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
getService(Ci.nsINavHistoryService);

function run_test()
{
  // QI to nsIAutoCompleteSimpleResultListener
  var listener = Cc["@mozilla.org/autocomplete/search;1?name=history"].
                 getService(Components.interfaces.nsIAutoCompleteSimpleResultListener);

  // add history visit
  var now = Date.now() * 1000;
  var uri = ios.newURI("http://foo.mozilla.com/", null, null);
  var ref = ios.newURI("http://mozilla.com/", null, null);
  var visit = histsvc.addVisit(uri, now, ref, 1, false, 0);
  // create a query object
  var query = histsvc.getNewQuery();
  // create the options object we will never use
  var options = histsvc.getNewQueryOptions();
  // look for this uri only
  query.uri = uri;
  // execute
  var queryRes = histsvc.executeQuery(query, options);
  // open the result container
  queryRes.root.containerOpen = true;
  // debug queries
  // dump_table("moz_places");
  do_check_eq(queryRes.root.childCount, 1);  
  // call the untested code path
  listener.onValueRemoved(null, uri.spec, true);
  // make sure it is GONE from the DB
  do_check_eq(queryRes.root.childCount, 0);
  // close the container
  queryRes.root.containerOpen = false;
}

