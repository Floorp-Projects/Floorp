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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com> (Original Author)
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

/**
 * Tests bookmark and history queries with tags.  See bug 399799.
 */

// Add your tests here.  Each is an object with a summary string |desc| and a
// method run() that's called to run the test.
var gTests = [
  {
    desc: "Tags getter/setter should work correctly",
    run:   function () {
      print("  Without setting tags, tags getter should return empty array");
      var [query, dummy] = makeQuery();
      do_check_eq(query.tags.length, 0);

      print("  Setting tags to an empty array, tags getter should return "+
            "empty array");
      [query, dummy] = makeQuery([]);
      do_check_eq(query.tags.length, 0);

      print("  Setting a few tags, tags getter should return correct array");
      var tags = ["bar", "baz", "foo"];
      [query, dummy] = makeQuery(tags);
      setsAreEqual(query.tags, tags, true);

      print("  Setting some dupe tags, tags getter return unique tags");
      [query, dummy] = makeQuery(["foo", "foo", "bar", "foo", "baz", "bar"]);
      setsAreEqual(query.tags, ["bar", "baz", "foo"], true);
    }
  },

  {
    desc: "Invalid calls to tags setter should fail",
    run:   function () {
      try {
        var query = histsvc.getNewQuery();
        query.tags = null;
        do_throw("  Passing null to SetTags should fail");
      }
      catch (exc) {}

      try {
        query = histsvc.getNewQuery();
        query.tags = "this should not work";
        do_throw("  Passing a string to SetTags should fail");
      }
      catch (exc) {}

      try {
        makeQuery([null]);
        do_throw("  Passing one-element array with null to SetTags should fail");
      }
      catch (exc) {}

      try {
        makeQuery([undefined]);
        do_throw("  Passing one-element array with undefined to SetTags " +
                 "should fail");
      }
      catch (exc) {}

      try {
        makeQuery(["foo", null, "bar"]);
        do_throw("  Passing mixture of tags and null to SetTags should fail");
      }
      catch (exc) {}

      try {
        makeQuery(["foo", undefined, "bar"]);
        do_throw("  Passing mixture of tags and undefined to SetTags " +
                 "should fail");
      }
      catch (exc) {}

      try {
        makeQuery([1, 2, 3]);
        do_throw("  Passing numbers to SetTags should fail");
      }
      catch (exc) {}

      try {
        makeQuery(["foo", 1, 2, 3]);
        do_throw("  Passing mixture of tags and numbers to SetTags should fail");
      }
      catch (exc) {}

      try {
        var str = Cc["@mozilla.org/supports-string;1"].
                  createInstance(Ci.nsISupportsString);
        str.data = "foo";
        query = histsvc.getNewQuery();
        query.tags = str;
        do_throw("  Passing nsISupportsString to SetTags should fail");
      }
      catch (exc) {}

      try {
        makeQuery([str]);
        do_throw("  Passing array of nsISupportsStrings to SetTags should fail");
      }
      catch (exc) {}
    }
  },

  {
    desc: "Not setting tags at all should not affect query URI",
    run:   function () {
      checkQueryURI();
    }
  },

  {
    desc: "Setting tags with an empty array should not affect query URI",
    run:   function () {
      checkQueryURI([]);
    }
  },

  {
    desc: "Setting some tags should result in correct query URI",
    run:   function () {
      checkQueryURI([
        "foo",
        "七難",
        "",
        "いっぱいおっぱい",
        "Abracadabra",
        "１２３",
        "Here's a pretty long tag name with some = signs and 1 2 3s and spaces oh jeez will it work I hope so!",
        "アスキーでございません",
        "あいうえお",
      ]);
    }
  },

  {
    desc: "Not setting tags at all but setting tagsAreNot should affect " +
          "query URI",
    run:   function () {
      checkQueryURI(null, true);
    }
  },

  {
    desc: "Setting tags with an empty array and setting tagsAreNot should " +
          "affect query URI",
    run:   function () {
      checkQueryURI([], true);
    }
  },

  {
    desc: "Setting some tags and setting tagsAreNot should result in correct " +
          "query URI",
    run:   function () {
      checkQueryURI([
        "foo",
        "七難",
        "",
        "いっぱいおっぱい",
        "Abracadabra",
        "１２３",
        "Here's a pretty long tag name with some = signs and 1 2 3s and spaces oh jeez will it work I hope so!",
        "アスキーでございません",
        "あいうえお",
      ], true);
    }
  },

  {
    desc: "Querying history on tag associated with a URI should return " +
          "that URI",
    run:   function () {
      doWithVisit(["foo", "bar", "baz"], function (aURI) {
        var [query, opts] = makeQuery(["foo"]);
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["bar"]);
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["baz"]);
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
      });
    }
  },

  {
    desc: "Querying history on many tags associated with a URI should " +
          "return that URI",
    run:   function () {
      doWithVisit(["foo", "bar", "baz"], function (aURI) {
        var [query, opts] = makeQuery(["foo", "bar"]);
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["foo", "baz"]);
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["bar", "baz"]);
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["foo", "bar", "baz"]);
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
      });
    }
  },

  {
    desc: "Specifying the same tag multiple times in a history query should " +
          "not matter",
    run:   function () {
      doWithVisit(["foo", "bar", "baz"], function (aURI) {
        var [query, opts] = makeQuery(["foo", "foo"]);
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["foo", "foo", "foo", "bar", "bar", "baz"]);
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
      });
    }
  },

  {
    desc: "Querying history on many tags associated with a URI and tags " +
          "not associated with that URI should not return that URI",
    run:   function () {
      doWithVisit(["foo", "bar", "baz"], function (aURI) {
        var [query, opts] = makeQuery(["foo", "bogus"]);
        executeAndCheckQueryResults(query, opts, []);
        [query, opts] = makeQuery(["foo", "bar", "bogus"]);
        executeAndCheckQueryResults(query, opts, []);
        [query, opts] = makeQuery(["foo", "bar", "baz", "bogus"]);
        executeAndCheckQueryResults(query, opts, []);
      });
    }
  },

  {
    desc: "Querying history on nonexisting tags should return no results",
    run:   function () {
      doWithVisit(["foo", "bar", "baz"], function (aURI) {
        var [query, opts] = makeQuery(["bogus"]);
        executeAndCheckQueryResults(query, opts, []);
        [query, opts] = makeQuery(["bogus", "gnarly"]);
        executeAndCheckQueryResults(query, opts, []);
      });
    }
  },

  {
    desc: "Querying bookmarks on tag associated with a URI should return " +
          "that URI",
    run:   function () {
      doWithBookmark(["foo", "bar", "baz"], function (aURI) {
        var [query, opts] = makeQuery(["foo"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["bar"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["baz"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
      });
    }
  },

  {
    desc: "Querying bookmarks on many tags associated with a URI should " +
          "return that URI",
    run:   function () {
      doWithBookmark(["foo", "bar", "baz"], function (aURI) {
        var [query, opts] = makeQuery(["foo", "bar"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["foo", "baz"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["bar", "baz"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["foo", "bar", "baz"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
      });
    }
  },

  {
    desc: "Specifying the same tag multiple times in a bookmark query should " +
          "not matter",
    run:   function () {
      doWithBookmark(["foo", "bar", "baz"], function (aURI) {
        var [query, opts] = makeQuery(["foo", "foo"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
        [query, opts] = makeQuery(["foo", "foo", "foo", "bar", "bar", "baz"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, [aURI.spec]);
      });
    }
  },

  {
    desc: "Querying bookmarks on many tags associated with a URI and tags " +
          "not associated with that URI should not return that URI",
    run:   function () {
      doWithBookmark(["foo", "bar", "baz"], function (aURI) {
        var [query, opts] = makeQuery(["foo", "bogus"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, []);
        [query, opts] = makeQuery(["foo", "bar", "bogus"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, []);
        [query, opts] = makeQuery(["foo", "bar", "baz", "bogus"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, []);
      });
    }
  },

  {
    desc: "Querying bookmarks on nonexisting tag should return no results",
    run:   function () {
      doWithBookmark(["foo", "bar", "baz"], function (aURI) {
        var [query, opts] = makeQuery(["bogus"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, []);
        [query, opts] = makeQuery(["bogus", "gnarly"]);
        opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
        executeAndCheckQueryResults(query, opts, []);
      });
    }
  },

  {
    desc: "Querying history using tagsAreNot should work correctly",
    run:   function () {
      var urisAndTags = {
        "http://example.com/1": ["foo", "bar"],
        "http://example.com/2": ["baz", "qux"],
        "http://example.com/3": null
      };

      print("  Add visits and tag the URIs");
      for (let [pURI, tags] in Iterator(urisAndTags)) {
        let nsiuri = uri(pURI);
        addVisit(nsiuri);
        if (tags)
          tagssvc.tagURI(nsiuri, tags);
      }

      print('  Querying for "foo" should match only /2 and /3');
      var [query, opts] = makeQuery(["foo"], true);
      queryResultsAre(histsvc.executeQuery(query, opts).root,
                      ["http://example.com/2", "http://example.com/3"]);

      print('  Querying for "foo" and "bar" should match only /2 and /3');
      [query, opts] = makeQuery(["foo", "bar"], true);
      queryResultsAre(histsvc.executeQuery(query, opts).root,
                      ["http://example.com/2", "http://example.com/3"]);

      print('  Querying for "foo" and "bogus" should match only /2 and /3');
      [query, opts] = makeQuery(["foo", "bogus"], true);
      queryResultsAre(histsvc.executeQuery(query, opts).root,
                      ["http://example.com/2", "http://example.com/3"]);

      print('  Querying for "foo" and "baz" should match only /3');
      [query, opts] = makeQuery(["foo", "baz"], true);
      queryResultsAre(histsvc.executeQuery(query, opts).root,
                      ["http://example.com/3"]);

      print('  Querying for "bogus" should match all');
      [query, opts] = makeQuery(["bogus"], true);
      queryResultsAre(histsvc.executeQuery(query, opts).root,
                      ["http://example.com/1",
                       "http://example.com/2",
                       "http://example.com/3"]);

      // Clean up.
      for (let [pURI, tags] in Iterator(urisAndTags)) {
        let nsiuri = uri(pURI);
        if (tags)
          tagssvc.untagURI(nsiuri, tags);
      }
      cleanDatabase();
    }
  },

  {
    desc: "Querying bookmarks using tagsAreNot should work correctly",
    run:   function () {
      var urisAndTags = {
        "http://example.com/1": ["foo", "bar"],
        "http://example.com/2": ["baz", "qux"],
        "http://example.com/3": null
      };

      print("  Add bookmarks and tag the URIs");
      for (let [pURI, tags] in Iterator(urisAndTags)) {
        let nsiuri = uri(pURI);
        addBookmark(nsiuri);
        if (tags)
          tagssvc.tagURI(nsiuri, tags);
      }

      print('  Querying for "foo" should match only /2 and /3');
      var [query, opts] = makeQuery(["foo"], true);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      queryResultsAre(histsvc.executeQuery(query, opts).root,
                      ["http://example.com/2", "http://example.com/3"]);

      print('  Querying for "foo" and "bar" should match only /2 and /3');
      [query, opts] = makeQuery(["foo", "bar"], true);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      queryResultsAre(histsvc.executeQuery(query, opts).root,
                      ["http://example.com/2", "http://example.com/3"]);

      print('  Querying for "foo" and "bogus" should match only /2 and /3');
      [query, opts] = makeQuery(["foo", "bogus"], true);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      queryResultsAre(histsvc.executeQuery(query, opts).root,
                      ["http://example.com/2", "http://example.com/3"]);

      print('  Querying for "foo" and "baz" should match only /3');
      [query, opts] = makeQuery(["foo", "baz"], true);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      queryResultsAre(histsvc.executeQuery(query, opts).root,
                      ["http://example.com/3"]);

      print('  Querying for "bogus" should match all');
      [query, opts] = makeQuery(["bogus"], true);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      queryResultsAre(histsvc.executeQuery(query, opts).root,
                      ["http://example.com/1",
                       "http://example.com/2",
                       "http://example.com/3"]);

      // Clean up.
      for (let [pURI, tags] in Iterator(urisAndTags)) {
        let nsiuri = uri(pURI);
        if (tags)
          tagssvc.untagURI(nsiuri, tags);
      }
      cleanDatabase();
    }
  },

  {
    desc: "Duplicate existing tags (i.e., multiple tag folders with same " +
          "name) should not throw off query results",
    run:   function () {
      var tagName = "foo";

      print("  Add bookmark and tag it normally");
      addBookmark(TEST_URI);
      tagssvc.tagURI(TEST_URI, [tagName]);

      print("  Manually create tag folder with same name as tag and insert " +
            "bookmark");
      var dupTagId = bmsvc.createFolder(bmsvc.tagsFolder,
                                        tagName,
                                        bmsvc.DEFAULT_INDEX);
      do_check_true(dupTagId > 0);
      var bmId = bmsvc.insertBookmark(dupTagId,
                                      TEST_URI,
                                      bmsvc.DEFAULT_INDEX,
                                      "title");
      do_check_true(bmId > 0);

      print("  Querying for tag should match URI");
      var [query, opts] = makeQuery([tagName]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      queryResultsAre(histsvc.executeQuery(query, opts).root, [TEST_URI.spec]);

      tagssvc.untagURI(TEST_URI, [tagName]);
      cleanDatabase();
    }
  },

  {
    desc: "Regular folders with the same name as tag should not throw off " +
          "query results",
    run:   function () {
      var tagName = "foo";

      print("  Add bookmark and tag it");
      addBookmark(TEST_URI);
      tagssvc.tagURI(TEST_URI, [tagName]);

      print("  Create folder with same name as tag");
      var folderId = bmsvc.createFolder(bmsvc.unfiledBookmarksFolder,
                                        tagName,
                                        bmsvc.DEFAULT_INDEX);
      do_check_true(folderId > 0);

      print("  Querying for tag should match URI");
      var [query, opts] = makeQuery([tagName]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      queryResultsAre(histsvc.executeQuery(query, opts).root, [TEST_URI.spec]);

      tagssvc.untagURI(TEST_URI, [tagName]);
      cleanDatabase();
    }
  },

  {
    desc: "Multiple queries ORed together should work",
    run:   function () {
      var urisAndTags = {
        "http://example.com/1": [],
        "http://example.com/2": []
      };

      // Search with lots of tags to make sure tag parameter substitution in SQL
      // can handle it with more than one query.
      for (let i = 0; i < 11; i++) {
        urisAndTags["http://example.com/1"].push("/1 tag " + i);
        urisAndTags["http://example.com/2"].push("/2 tag " + i);
      }

      print("  Add visits and tag the URIs");
      for (let [pURI, tags] in Iterator(urisAndTags)) {
        let nsiuri = uri(pURI);
        addVisit(nsiuri);
        if (tags)
          tagssvc.tagURI(nsiuri, tags);
      }

      print("  Query for /1 OR query for /2 should match both /1 and /2");
      var [query1, opts] = makeQuery(urisAndTags["http://example.com/1"]);
      var [query2, dummy] = makeQuery(urisAndTags["http://example.com/2"]);
      var root = histsvc.executeQueries([query1, query2], 2, opts).root;
      queryResultsAre(root, ["http://example.com/1", "http://example.com/2"]);

      print("  Query for /1 OR query on bogus tag should match only /1");
      [query1, opts] = makeQuery(urisAndTags["http://example.com/1"]);
      [query2, dummy] = makeQuery(["bogus"]);
      root = histsvc.executeQueries([query1, query2], 2, opts).root;
      queryResultsAre(root, ["http://example.com/1"]);

      print("  Query for /1 OR query for /1 should match only /1");
      [query1, opts] = makeQuery(urisAndTags["http://example.com/1"]);
      [query2, dummy] = makeQuery(urisAndTags["http://example.com/1"]);
      root = histsvc.executeQueries([query1, query2], 2, opts).root;
      queryResultsAre(root, ["http://example.com/1"]);

      print("  Query for /1 with tagsAreNot OR query for /2 with tagsAreNot " +
            "should match both /1 and /2");
      [query1, opts] = makeQuery(urisAndTags["http://example.com/1"], true);
      [query2, dummy] = makeQuery(urisAndTags["http://example.com/2"], true);
      root = histsvc.executeQueries([query1, query2], 2, opts).root;
      queryResultsAre(root, ["http://example.com/1", "http://example.com/2"]);

      print("  Query for /1 OR query for /2 with tagsAreNot should match " +
            "only /1");
      [query1, opts] = makeQuery(urisAndTags["http://example.com/1"]);
      [query2, dummy] = makeQuery(urisAndTags["http://example.com/2"], true);
      root = histsvc.executeQueries([query1, query2], 2, opts).root;
      queryResultsAre(root, ["http://example.com/1"]);

      print("  Query for /1 OR query for /1 with tagsAreNot should match " +
            "both URIs");
      [query1, opts] = makeQuery(urisAndTags["http://example.com/1"]);
      [query2, dummy] = makeQuery(urisAndTags["http://example.com/1"], true);
      root = histsvc.executeQueries([query1, query2], 2, opts).root;
      queryResultsAre(root, ["http://example.com/1", "http://example.com/2"]);

      // Clean up.
      for (let [pURI, tags] in Iterator(urisAndTags)) {
        let nsiuri = uri(pURI);
        if (tags)
          tagssvc.untagURI(nsiuri, tags);
      }
      cleanDatabase();
    }
  },
];

// The tag keys in query URIs, i.e., "place:tag=foo&!tags=1"
//                                          ---     -----
const QUERY_KEY_TAG      = "tag";
const QUERY_KEY_NOT_TAGS = "!tags";

const TEST_URI = uri("http://example.com/");

///////////////////////////////////////////////////////////////////////////////

/**
 * Adds a bookmark.
 *
 * @param aURI
 *        URI of the page (an nsIURI)
 */
function addBookmark(aURI) {
  var bmId = bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder,
                                  aURI,
                                  bmsvc.DEFAULT_INDEX,
                                  aURI.spec);
  print("  Sanity check: insertBookmark should not fail");
  do_check_true(bmId > 0);
}

/**
 * Adds a visit to history.
 *
 * @param aURI
 *        URI of the page (an nsIURI)
 */
function addVisit(aURI) {
  var visitId = histsvc.addVisit(aURI,
                                 Date.now() * 1000,
                                 null,
                                 histsvc.TRANSITION_LINK,
                                 false,
                                 0);
  print("  Sanity check: addVisit should not fail");
  do_check_true(visitId > 0);
}

/**
 * Removes all pages from history and bookmarks.
 */
function cleanDatabase() {
  bhistsvc.removeAllPages();
  remove_all_bookmarks();
}

/**
 * Sets up a query with the specified tags, converts it to a URI, and makes sure
 * the URI is what we expect it to be.
 *
 * @param aTags
 *        The query's tags will be set to those in this array
 * @param aTagsAreNot
 *        The query's tagsAreNot property will be set to this
 */
function checkQueryURI(aTags, aTagsAreNot) {
  var pairs = (aTags || []).sort().map(function (t) QUERY_KEY_TAG + "=" + encodeTag(t));
  if (aTagsAreNot)
    pairs.push(QUERY_KEY_NOT_TAGS + "=1");
  var expURI = "place:" + pairs.join("&");
  var [query, opts] = makeQuery(aTags, aTagsAreNot);
  var actualURI = queryURI(query, opts);
  print("  Query URI should be what we expect for the given tags");
  do_check_eq(actualURI, expURI);
}

/**
 * Executes a callback function in a "scoped" database state.  A bookmark
 * is added and tagged before the callback is called, and afterward the
 * database is cleared.
 *
 * @param aTags
 *        A bookmark will be added and tagged with this array of tags
 * @param aCallback
 *        A function that will be called after the bookmark has been tagged
 */
function doWithBookmark(aTags, aCallback) {
  addBookmark(TEST_URI);
  tagssvc.tagURI(TEST_URI, aTags);
  aCallback(TEST_URI);
  tagssvc.untagURI(TEST_URI, aTags);
  cleanDatabase();
}

/**
 * Executes a callback function in a "scoped" database state.  A history visit
 * is added and tagged before the callback is called, and afterward the
 * database is cleared.
 *
 * @param aTags
 *        A history visit will be added and tagged with this array of tags
 * @param aCallback
 *        A function that will be called after the visit has been tagged
 */
function doWithVisit(aTags, aCallback) {
  addVisit(TEST_URI);
  tagssvc.tagURI(TEST_URI, aTags);
  aCallback(TEST_URI);
  tagssvc.untagURI(TEST_URI, aTags);
  cleanDatabase();
}

/**
 * queriesToQueryString() encodes every character in the query URI that doesn't
 * match /[a-zA-Z]/.  There's no simple JavaScript function that does the same,
 * but encodeURIComponent() comes close, only missing some punctuation.  This
 * function takes care of all of that.
 *
 * @param  aTag
 *         A tag name to encode
 * @return A UTF-8 escaped string suitable for inclusion in a query URI
 */
function encodeTag(aTag) {
  return encodeURIComponent(aTag).
         replace(/[-_.!~*'()]/g, //'
                 function (s) "%" + s.charCodeAt(0).toString(16));
}

/**
 * Executes the given query and compares the results to the given URIs.
 * See queryResultsAre().
 *
 * @param aQuery
 *        An nsINavHistoryQuery
 * @param aQueryOpts
 *        An nsINavHistoryQueryOptions
 * @param aExpectedURIs
 *        Array of URIs (as strings) that aResultRoot should contain
 */
function executeAndCheckQueryResults(aQuery, aQueryOpts, aExpectedURIs) {
  var root = histsvc.executeQuery(aQuery, aQueryOpts).root;
  root.containerOpen = true;
  queryResultsAre(root, aExpectedURIs);
  root.containerOpen = false;
}

/**
 * Returns new query and query options objects.  The query's tags will be
 * set to aTags.  aTags may be null, in which case setTags() is not called at
 * all on the query.
 *
 * @param  aTags
 *         The query's tags will be set to those in this array
 * @param  aTagsAreNot
 *         The query's tagsAreNot property will be set to this
 * @return [query, queryOptions]
 */
function makeQuery(aTags, aTagsAreNot) {
  aTagsAreNot = !!aTagsAreNot;
  print("  Making a query " +
        (aTags ?
         "with tags " + aTags.toSource() :
         "without calling setTags() at all") +
        " and with tagsAreNot=" +
        aTagsAreNot);
  var query = histsvc.getNewQuery();
  query.tagsAreNot = aTagsAreNot;
  if (aTags) {
    query.tags = aTags;
    var uniqueTags = [];
    aTags.forEach(function (t) {
      if (typeof(t) === "string" && uniqueTags.indexOf(t) < 0)
        uniqueTags.push(t);
    });
    uniqueTags.sort();
  }

  print("  Made query should be correct for tags and tagsAreNot");
  if (uniqueTags)
    setsAreEqual(query.tags, uniqueTags, true);
  var expCount = uniqueTags ? uniqueTags.length : 0;
  do_check_eq(query.tags.length, expCount);
  do_check_eq(query.tagsAreNot, aTagsAreNot);

  return [query, histsvc.getNewQueryOptions()];
}

/**
 * Ensures that the URIs of aResultRoot are the same as those in aExpectedURIs.
 *
 * @param aResultRoot
 *        The nsINavHistoryContainerResultNode root of an nsINavHistoryResult
 * @param aExpectedURIs
 *        Array of URIs (as strings) that aResultRoot should contain
 */
function queryResultsAre(aResultRoot, aExpectedURIs) {
  var rootWasOpen = aResultRoot.containerOpen;
  if (!rootWasOpen)
    aResultRoot.containerOpen = true;
  var actualURIs = [];
  for (let i = 0; i < aResultRoot.childCount; i++) {
    actualURIs.push(aResultRoot.getChild(i).uri);
  }
  setsAreEqual(actualURIs, aExpectedURIs);
  if (!rootWasOpen)
    aResultRoot.containerOpen = false;
}

/**
 * Converts the given query into its query URI.
 *
 * @param  aQuery
 *         An nsINavHistoryQuery
 * @param  aQueryOpts
 *         An nsINavHistoryQueryOptions
 * @return The query's URI
 */
function queryURI(aQuery, aQueryOpts) {
  return histsvc.queriesToQueryString([aQuery], 1, aQueryOpts);
}

/**
 * Ensures that the arrays contain the same elements and, optionally, in the
 * same order.
 */
function setsAreEqual(aArr1, aArr2, aIsOrdered) {
  do_check_eq(aArr1.length, aArr2.length);
  if (aIsOrdered) {
    for (let i = 0; i < aArr1.length; i++) {
      do_check_eq(aArr1[i], aArr2[i]);
    }
  }
  else {
    aArr1.forEach(function (u) do_check_true(aArr2.indexOf(u) >= 0));
    aArr2.forEach(function (u) do_check_true(aArr1.indexOf(u) >= 0));
  }
}

///////////////////////////////////////////////////////////////////////////////

function run_test() {
  cleanDatabase();
  gTests.forEach(function (t) {
    print("Running test: " + t.desc);
    t.run()
  });
  cleanDatabase();
}
