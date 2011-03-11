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
 * The Original Code is Bug 451499 code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77bonardo.net> (Original Author)
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
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                  getService(Ci.nsINavHistoryService);
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                getService(Ci.nsINavBookmarksService);
  var iconsvc = Cc["@mozilla.org/browser/favicon-service;1"].
                  getService(Ci.nsIFaviconService);
} catch(ex) {
  do_throw("Could not get services\n");
}

/*
 * readFileData()
 *
 * Reads the data from the specified nsIFile, and returns an array of bytes.
 */
function readFileData(aFile) {
  var inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                    createInstance(Ci.nsIFileInputStream);
  // init the stream as RD_ONLY, -1 == default permissions.
  inputStream.init(aFile, 0x01, -1, null);
  var size = inputStream.available();

  // use a binary input stream to grab the bytes.
  var bis = Cc["@mozilla.org/binaryinputstream;1"].
            createInstance(Ci.nsIBinaryInputStream);
  bis.setInputStream(inputStream);

  var bytes = bis.readByteArray(size);

  if (size != bytes.length)
      throw "Didn't read expected number of bytes";

  return bytes;
}

var result;
var resultObserver = {
  itemChanged: function(item) {
    // The favicon should not be set on the containing query.
    if (item.uri.substr(0,5) == "place")
      print("Testing itemChanged on: " + item.uri);
    do_check_eq(item.icon.spec, null);
  }
};

// main
function run_test() {
  var testURI = uri("http://places.test/");

  // Setup a real favicon data
  var iconName = "favicon-normal16.png";
  var iconURI = uri("http://places.test/" + iconName);
  var iconMimeType = "image/png";
  var iconFile = do_get_file(iconName);
  var iconData = readFileData(iconFile);
  do_check_eq(iconData.length, 286);
  iconsvc.setFaviconData(iconURI,
                         iconData, iconData.length, iconMimeType,
                         Number.MAX_VALUE);

  // Bookmark our test page, so it will appear in the query resultset
  var testBookmark = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder,
                                          testURI,
                                          bmsvc.DEFAULT_INDEX,
                                          "foo");

  // Get the last 10 bookmarks added to menu or toolbar
  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  query.setFolders([bmsvc.bookmarksMenuFolder, bmsvc.toolbarFolder], 2);
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.maxResults = 10;
  options.excludeQueries = 1;
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  result = histsvc.executeQuery(query, options);
  result.addObserver(resultObserver, false);
  
  var root = result.root;
  root.containerOpen = true;

  // We set a favicon on testURI while the container is open.
  // This favicon should be setup only for this website, not for the containing
  // query, this will be checked by the viewer
  iconsvc.setFaviconUrlForPage(testURI, iconURI);

  do_test_pending();
  // lazy timeout is 3s and favicons are lazy added
  do_timeout(3500, end_test);
}

function end_test() {
  var root = result.root;
  root.containerOpen = false;
  result.removeObserver(resultObserver);

  do_test_finished();
}
