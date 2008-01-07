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
  var bhist = Cc["@mozilla.org/browser/global-history;2"].getService(Ci.nsIBrowserHistory);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// main
function run_test() {
  var testURI = uri("http://mozilla.com");

  /** 
   * addPageWithDetails
   * Adds a page to history with specific time stamp information. This is used in
   * the History migrator. 
   */
  try {
    bhist.addPageWithDetails(testURI, "testURI", Date.now());
  } catch(ex) {
    do_throw("addPageWithDetails failed");
  }

  /**
   * lastPageVisited
   * The title of the last page that was visited in a top-level window.
   */
  do_check_eq("http://mozilla.com/", bhist.lastPageVisited);

  /**
   * count
   * The number of entries in global history
   */
  do_check_eq(1, bhist.count);

  /**
   * remove a page from history
   */
  try {
    bhist.removePage(testURI);
  } catch(ex) {
    do_throw("removePage failed");
  }
  do_check_eq(0, bhist.count);
  do_check_eq("", bhist.lastPageVisited);

  /**
   * removePagesFromHost
   * Remove all pages from the given host.
   * If aEntireDomain is true, will assume aHost is a domain,
   * and remove all pages from the entire domain.
   */
  bhist.addPageWithDetails(testURI, "testURI", Date.now());
  bhist.removePagesFromHost("mozilla.com", true);
  do_check_eq(0, bhist.count);

  // test aEntireDomain
  bhist.addPageWithDetails(testURI, "testURI", Date.now());
  bhist.addPageWithDetails(uri("http://foobar.mozilla.com"), "testURI2", Date.now());
  bhist.removePagesFromHost("mozilla.com", false);
  do_check_eq(1, bhist.count);

  /**
   * removeAllPages
   * Remove all pages from global history
   */
  bhist.removeAllPages();
  do_check_eq(0, bhist.count);

  /**
   * hidePage
   * Hide the specified URL from being enumerated (and thus
   * displayed in the UI)
   *
   * if the page hasn't been visited yet, then it will be added
   * as if it was visited, and then marked as hidden
   */
  //XXX NOT IMPLEMENTED in the history service
  //bhist.addPageWithDetails(testURI, "testURI", Date.now());
  //bhist.hidePage(testURI);
  //do_check_eq(0, bhist.count);
}
