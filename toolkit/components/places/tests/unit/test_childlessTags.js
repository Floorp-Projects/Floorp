/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Ensures that removal of a bookmark untags the bookmark if it's no longer
 * contained in any regular, non-tag folders.  See bug 444849.
 */

// Add your tests here.  Each is an object with a summary string |desc| and a
// method run() that's called to run the test.
var tests = [
  {
    desc: "Removing a tagged bookmark should cause the tag to be removed.",
    run:   function () {
      print("  Make a bookmark.");
      var bmId = bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder,
                                      BOOKMARK_URI,
                                      bmsvc.DEFAULT_INDEX,
                                      "test bookmark");
      do_check_true(bmId > 0);

      print("  Tag it up.");
      var tags = ["foo", "bar"];
      tagssvc.tagURI(BOOKMARK_URI, tags);
      ensureTagsExist(tags);

      print("  Remove the bookmark.  The tags should no longer exist.");
      bmsvc.removeItem(bmId);
      ensureTagsExist([]);
    }
  },

  {
    desc: "Removing a folder containing a tagged bookmark should cause the " +
          "tag to be removed.",
    run:   function () {
      print("  Make a folder.");
      var folderId = bmsvc.createFolder(bmsvc.unfiledBookmarksFolder,
                                        "test folder",
                                        bmsvc.DEFAULT_INDEX);
      do_check_true(folderId > 0);

      print("  Stick a bookmark in the folder.");
      var bmId = bmsvc.insertBookmark(folderId,
                                      BOOKMARK_URI,
                                      bmsvc.DEFAULT_INDEX,
                                      "test bookmark");
      do_check_true(bmId > 0);

      print("  Tag the bookmark.");
      var tags = ["foo", "bar"];
      tagssvc.tagURI(BOOKMARK_URI, tags);
      ensureTagsExist(tags);

      print("  Remove the folder.  The tags should no longer exist.");
      bmsvc.removeItem(folderId);
      ensureTagsExist([]);
    }
  }
];

var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
            getService(Ci.nsINavBookmarksService);

var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
              getService(Ci.nsINavHistoryService);

var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
              getService(Ci.nsITaggingService);

const BOOKMARK_URI = uri("http://example.com/");

/**
 * Runs a tag query and ensures that the tags returned are those and only those
 * in aTags.  aTags may be empty, in which case this function ensures that no
 * tags exist.
 *
 * @param aTags
 *        An array of tags (strings)
 */
function ensureTagsExist(aTags) {
  var query = histsvc.getNewQuery();
  var opts = histsvc.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_TAG_QUERY;
  var resultRoot = histsvc.executeQuery(query, opts).root;

  // Dupe aTags.
  var tags = aTags.slice(0);

  resultRoot.containerOpen = true;

  // Ensure that the number of tags returned from the query is the same as the
  // number in |tags|.
  do_check_eq(resultRoot.childCount, tags.length);

  // For each tag result from the query, ensure that it's contained in |tags|.
  // Remove the tag from |tags| so that we ensure the sets are equal.
  for (let i = 0; i < resultRoot.childCount; i++) {
    var tag = resultRoot.getChild(i).title;
    var indexOfTag = tags.indexOf(tag);
    do_check_true(indexOfTag >= 0);
    tags.splice(indexOfTag, 1);
  }

  resultRoot.containerOpen = false;
}

function run_test()
{
  tests.forEach(function (test) {
    print("Running test: " + test.desc);
    test.run();
  });
}
