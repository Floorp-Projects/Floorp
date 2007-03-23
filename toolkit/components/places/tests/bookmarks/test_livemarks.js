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

// Get livemark service
try {
  var lmsvc = Cc["@mozilla.org/browser/livemark-service;2"].
              getService(Ci.nsILivemarkService);
} catch(ex) {
  do_throw("Could not get livemark-service\n");
} 

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

// get bookmarks root index
var root = bmsvc.bookmarksRoot;

// main
function run_test() {
  var livemarkId = 
    lmsvc.createLivemarkFolderOnly(bmsvc, root, "foo",
                                   uri("http://example.com/"),
                                   uri("http://example.com/rss.xml"), -1);


  try {
    lmsvc.QueryInterface(Ci.nsIRemoteContainer);
  } catch(ex) {
    do_throw("Failed to QueryInterface livemark-service to nsIRemoteContainer");
  }

  do_check_true(lmsvc.childrenReadOnly);

  do_check_true(lmsvc.isLivemark(livemarkId));
  do_check_true(lmsvc.getSiteURI(livemarkId).spec == "http://example.com/");
  do_check_true(lmsvc.getFeedURI(livemarkId).spec == "http://example.com/rss.xml");

  lmsvc.setSiteURI(livemarkId, uri("http://foo.example.com/"));
  do_check_true(lmsvc.getSiteURI(livemarkId).spec == "http://foo.example.com/");
  
  lmsvc.setFeedURI(livemarkId, uri("http://foo.example.com/rss.xml"));
  do_check_true(lmsvc.getFeedURI(livemarkId).spec == "http://foo.example.com/rss.xml");

  // Make sure we can't add a livemark to a livemark
  var livemarkId2 = null;
  try {
    var livemarkId2 = lmsvc.createLivemark(livemarkId, "foo", uri("http://example.com/"), 
                                           uri("http://example.com/rss.xml"), -1);
  } catch (ex) {
    livemarkId2 = null;
  }
  do_check_true(livemarkId2 == null);
  
  // make sure it didn't screw up the first one
  do_check_true(lmsvc.isLivemark(livemarkId));
  
  // make sure folders don't get counted as bookmarks
  // create folder
  var randomFolder = bmsvc.createFolder(root, "Random", -1);
  do_check_true(!lmsvc.isLivemark(randomFolder));
}
