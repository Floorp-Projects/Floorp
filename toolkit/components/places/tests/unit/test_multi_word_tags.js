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
 * The Original Code is Places Tagging Service unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozilla.com> (Original Author)
 *   Seth Spitzer <sspitzer@mozilla.com>
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

// Get tagging service
try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
                getService(Ci.nsITaggingService);
} catch(ex) {
  do_throw("Could not get tagging service\n");
}

// main
function run_test() {
  var uri1 = uri("http://site.tld/1");
  var uri2 = uri("http://site.tld/2");
  var uri3 = uri("http://site.tld/3");
  var uri4 = uri("http://site.tld/4");
  var uri5 = uri("http://site.tld/5");
  var uri6 = uri("http://site.tld/6");

  tagssvc.tagURI(uri1, ["foo"]);
  tagssvc.tagURI(uri2, ["bar"]);
  tagssvc.tagURI(uri3, ["cheese"]);
  tagssvc.tagURI(uri4, ["foo bar"]);
  tagssvc.tagURI(uri5, ["bar cheese"]);
  tagssvc.tagURI(uri6, ["foo bar cheese"]);

  // exclude livemark items, search for "item", should get one result
  var options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  var query = histsvc.getNewQuery();
  query.searchTerms = "foo";
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 1);
  do_check_eq(root.getChild(0).uri, "http://site.tld/1");

  query.searchTerms = "bar";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 1);
  do_check_eq(root.getChild(0).uri, "http://site.tld/2");

  query.searchTerms = "cheese";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 1);
  do_check_eq(root.getChild(0).uri, "http://site.tld/3");

  query.searchTerms = "foo bar";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 3);
  do_check_eq(root.getChild(0).uri, "http://site.tld/1");
  do_check_eq(root.getChild(1).uri, "http://site.tld/2");
  do_check_eq(root.getChild(2).uri, "http://site.tld/4");

  query.searchTerms = "bar foo";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 2);
  do_check_eq(root.getChild(0).uri, "http://site.tld/1");
  do_check_eq(root.getChild(1).uri, "http://site.tld/2");

  query.searchTerms = "bar cheese";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 3);
  do_check_eq(root.getChild(0).uri, "http://site.tld/2");
  do_check_eq(root.getChild(1).uri, "http://site.tld/3");
  do_check_eq(root.getChild(2).uri, "http://site.tld/5");

  query.searchTerms = "cheese bar";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 2);
  do_check_eq(root.getChild(0).uri, "http://site.tld/2");
  do_check_eq(root.getChild(1).uri, "http://site.tld/3");

  query.searchTerms = "foo bar cheese";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 6);
  do_check_eq(root.getChild(0).uri, "http://site.tld/1");
  do_check_eq(root.getChild(1).uri, "http://site.tld/2");
  do_check_eq(root.getChild(2).uri, "http://site.tld/3");
  do_check_eq(root.getChild(3).uri, "http://site.tld/4");
  do_check_eq(root.getChild(4).uri, "http://site.tld/5");
  do_check_eq(root.getChild(5).uri, "http://site.tld/6");

  query.searchTerms = "cheese foo bar";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 4);
  do_check_eq(root.getChild(0).uri, "http://site.tld/1");
  do_check_eq(root.getChild(1).uri, "http://site.tld/2");
  do_check_eq(root.getChild(2).uri, "http://site.tld/3");
  do_check_eq(root.getChild(3).uri, "http://site.tld/4");

  query.searchTerms = "cheese bar foo";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 3);
  do_check_eq(root.getChild(0).uri, "http://site.tld/1");
  do_check_eq(root.getChild(1).uri, "http://site.tld/2");
  do_check_eq(root.getChild(2).uri, "http://site.tld/3");
}
