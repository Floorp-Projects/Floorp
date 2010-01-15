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
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@supereva.it>
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

const charset = "UTF-8";
const CHARSET_ANNO = "URIProperties/characterSet";

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
  var annosvc = Cc["@mozilla.org/browser/annotation-service;1"].
                getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get services\n");
}

const TEST_URI = uri("http://foo.com");
const TEST_BOOKMARKED_URI = uri("http://bar.com");

function run_test() {
  var now = Date.now();

  // add pages to history
  histsvc.addVisit(TEST_URI, now, null,
                   Ci.nsINavHistoryService.TRANSITION_TYPED,
                   false, 0);
  histsvc.addVisit(TEST_BOOKMARKED_URI, now, null,
                   Ci.nsINavHistoryService.TRANSITION_TYPED,
                   false, 0);

  // create bookmarks on TEST_BOOKMARKED_URI
  var bm1 = bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder,
                                 TEST_BOOKMARKED_URI, bmsvc.DEFAULT_INDEX,
                                 TEST_BOOKMARKED_URI.spec);
  var bm2 = bmsvc.insertBookmark(bmsvc.toolbarFolder,
                                 TEST_BOOKMARKED_URI, bmsvc.DEFAULT_INDEX,
                                 TEST_BOOKMARKED_URI.spec);

  // set charset on not-bookmarked page
  histsvc.setCharsetForURI(TEST_URI, charset);
  // set charset on bookmarked page
  histsvc.setCharsetForURI(TEST_BOOKMARKED_URI, charset);

  // check that we have created a page annotation
  do_check_eq(annosvc.getPageAnnotation(TEST_URI, CHARSET_ANNO), charset);

  // get charset from not-bookmarked page
  do_check_eq(histsvc.getCharsetForURI(TEST_URI), charset);
  // get charset from bookmarked page
  do_check_eq(histsvc.getCharsetForURI(TEST_BOOKMARKED_URI), charset);

  waitForClearHistory(continue_test);

  do_test_pending();
}

function continue_test() {
  // ensure that charset has gone for not-bookmarked page
  do_check_neq(histsvc.getCharsetForURI(TEST_URI), charset);

  // check that page annotation has been removed
  try {
    annosvc.getPageAnnotation(TEST_URI, CHARSET_ANNO);
    do_throw("Charset page annotation has not been removed correctly");
  } catch (e) {}

  // ensure that charset still exists for bookmarked page
  do_check_eq(histsvc.getCharsetForURI(TEST_BOOKMARKED_URI), charset);

  // remove charset from bookmark and check that has gone
  histsvc.setCharsetForURI(TEST_BOOKMARKED_URI, "");
  do_check_neq(histsvc.getCharsetForURI(TEST_BOOKMARKED_URI), charset);

  do_test_finished();
}
