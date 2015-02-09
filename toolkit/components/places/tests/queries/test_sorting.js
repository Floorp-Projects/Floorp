/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var tests = [];

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_NONE,

  setup: function() {
    LOG("Sorting test 1: SORT BY NONE");

    this._unsortedData = [
      { isBookmark: true,
        uri: "http://example.com/b",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "y",
        keyword: "b",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/a",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "z",
        keyword: "a",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/c",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "x",
        keyword: "c",
        isInQuery: true },
    ];

    this._sortedData = this._unsortedData;

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
    query.onlyBookmarked = true;

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    // no reverse sorting for SORT BY NONE
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING,

  setup: function() {
    LOG("Sorting test 2: SORT BY TITLE");

    this._unsortedData = [
      { isBookmark: true,
        uri: "http://example.com/b1",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "y",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/a",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "z",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/c",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "x",
        isInQuery: true },

      // if titles are equal, should fall back to URI
      { isBookmark: true,
        uri: "http://example.com/b2",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "y",
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[2],
      this._unsortedData[0],
      this._unsortedData[3],
      this._unsortedData[1],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
    query.onlyBookmarked = true;

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_ASCENDING,

  setup: function() {
    LOG("Sorting test 3: SORT BY DATE");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isVisit: true,
        isDetails: true,
        isBookmark: true,
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 0,
        uri: "http://example.com/c1",
        lastVisit: timeInMicroseconds - 2,
        title: "x1",
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        isBookmark: true,
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 1,
        uri: "http://example.com/a",
        lastVisit: timeInMicroseconds - 1,
        title: "z",
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        isBookmark: true,
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 2,
        uri: "http://example.com/b",
        lastVisit: timeInMicroseconds - 3,
        title: "y",
        isInQuery: true },

      // if dates are equal, should fall back to title
      { isVisit: true,
        isDetails: true,
        isBookmark: true,
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 3,
        uri: "http://example.com/c2",
        lastVisit: timeInMicroseconds - 2,
        title: "x2",
        isInQuery: true },

      // if dates and title are equal, should fall back to bookmark index
      { isVisit: true,
        isDetails: true,
        isBookmark: true,
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 4,
        uri: "http://example.com/c2",
        lastVisit: timeInMicroseconds - 2,
        title: "x2",
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[2],
      this._unsortedData[0],
      this._unsortedData[3],
      this._unsortedData[4],
      this._unsortedData[1],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
    query.onlyBookmarked = true;

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_URI_ASCENDING,

  setup: function() {
    LOG("Sorting test 4: SORT BY URI");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isBookmark: true,
        isDetails: true,
        lastVisit: timeInMicroseconds,
        uri: "http://example.com/b",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 0,
        title: "y",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/c",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 1,
        title: "x",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/a",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 2,
        title: "z",
        isInQuery: true },

      // if URIs are equal, should fall back to date
      { isBookmark: true,
        isDetails: true,
        lastVisit: timeInMicroseconds + 1000,
        uri: "http://example.com/c",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 3,
        title: "x",
        isInQuery: true },

      // if no URI (e.g., node is a folder), should fall back to title
      { isFolder: true,
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 4,
        title: "a",
        isInQuery: true },

      // if URIs and dates are equal, should fall back to bookmark index
      { isBookmark: true,
        isDetails: true,
        lastVisit: timeInMicroseconds + 1000,
        uri: "http://example.com/c",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 5,
        title: "x",
        isInQuery: true },

      // if no URI and titles are equal, should fall back to bookmark index
      { isFolder: true,
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 6,
        title: "a",
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[4],
      this._unsortedData[6],
      this._unsortedData[2],
      this._unsortedData[0],
      this._unsortedData[1],
      this._unsortedData[3],
      this._unsortedData[5],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_URI_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_ASCENDING,

  setup: function() {
    LOG("Sorting test 5: SORT BY VISITCOUNT");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isBookmark: true,
        uri: "http://example.com/a",
        lastVisit: timeInMicroseconds,
        title: "z",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 0,
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/c",
        lastVisit: timeInMicroseconds,
        title: "x",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 1,
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/b1",
        lastVisit: timeInMicroseconds,
        title: "y1",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 2,
        isInQuery: true },

      // if visitCounts are equal, should fall back to date
      { isBookmark: true,
        uri: "http://example.com/b2",
        lastVisit: timeInMicroseconds + 1000,
        title: "y2a",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 3,
        isInQuery: true },

      // if visitCounts and dates are equal, should fall back to bookmark index
      { isBookmark: true,
        uri: "http://example.com/b2",
        lastVisit: timeInMicroseconds + 1000,
        title: "y2b",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 4,
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[0],
      this._unsortedData[2],
      this._unsortedData[3],
      this._unsortedData[4],
      this._unsortedData[1],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
    // add visits to increase visit count
    yield PlacesTestUtils.addVisits([
      { uri: uri("http://example.com/a"), transition: TRANSITION_TYPED, visitDate: timeInMicroseconds },
      { uri: uri("http://example.com/b1"), transition: TRANSITION_TYPED, visitDate: timeInMicroseconds },
      { uri: uri("http://example.com/b1"), transition: TRANSITION_TYPED, visitDate: timeInMicroseconds },
      { uri: uri("http://example.com/b2"), transition: TRANSITION_TYPED, visitDate: timeInMicroseconds + 1000 },
      { uri: uri("http://example.com/b2"), transition: TRANSITION_TYPED, visitDate: timeInMicroseconds + 1000 },
      { uri: uri("http://example.com/c"), transition: TRANSITION_TYPED, visitDate: timeInMicroseconds },
      { uri: uri("http://example.com/c"), transition: TRANSITION_TYPED, visitDate: timeInMicroseconds },
      { uri: uri("http://example.com/c"), transition: TRANSITION_TYPED, visitDate: timeInMicroseconds },
    ]);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
    query.onlyBookmarked = true;

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_KEYWORD_ASCENDING,

  setup: function() {
    LOG("Sorting test 6: SORT BY KEYWORD");

    this._unsortedData = [
      { isBookmark: true,
        uri: "http://example.com/a",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "z",
        keyword: "a",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/c",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "x",
        keyword: "c",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/b1",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "y9",
        keyword: "b",
        isInQuery: true },

      // without a keyword, should fall back to title
      { isBookmark: true,
        uri: "http://example.com/null2",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "null8",
        keyword: null,
        isInQuery: true },

      // without a keyword, should fall back to title
      { isBookmark: true,
        uri: "http://example.com/null1",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "null9",
        keyword: null,
        isInQuery: true },

      // if keywords are equal, should fall back to title
      { isBookmark: true,
        uri: "http://example.com/b2",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "y8",
        keyword: "b",
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[3],
      this._unsortedData[4],
      this._unsortedData[0],
      this._unsortedData[5],
      this._unsortedData[2],
      this._unsortedData[1],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
    query.onlyBookmarked = true;

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_KEYWORD_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_ASCENDING,

  setup: function() {
    LOG("Sorting test 7: SORT BY DATEADDED");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isBookmark: true,
        uri: "http://example.com/b1",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 0,
        title: "y1",
        dateAdded: timeInMicroseconds - 1000,
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/a",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 1,
        title: "z",
        dateAdded: timeInMicroseconds - 2000,
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/c",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 2,
        title: "x",
        dateAdded: timeInMicroseconds,
        isInQuery: true },

      // if dateAddeds are equal, should fall back to title
      { isBookmark: true,
        uri: "http://example.com/b2",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 3,
        title: "y2",
        dateAdded: timeInMicroseconds - 1000,
        isInQuery: true },

      // if dateAddeds and titles are equal, should fall back to bookmark index
      { isBookmark: true,
        uri: "http://example.com/b3",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 4,
        title: "y3",
        dateAdded: timeInMicroseconds - 1000,
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[3],
      this._unsortedData[4],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
    query.onlyBookmarked = true;

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_LASTMODIFIED_ASCENDING,

  setup: function() {
    LOG("Sorting test 8: SORT BY LASTMODIFIED");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isBookmark: true,
        uri: "http://example.com/b1",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 0,
        title: "y1",
        lastModified: timeInMicroseconds - 1000,
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/a",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 1,
        title: "z",
        lastModified: timeInMicroseconds - 2000,
        isInQuery: true },

      { isBookmark: true,
        uri: "http://example.com/c",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 2,
        title: "x",
        lastModified: timeInMicroseconds,
        isInQuery: true },

      // if lastModifieds are equal, should fall back to title
      { isBookmark: true,
        uri: "http://example.com/b2",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 3,
        title: "y2",
        lastModified: timeInMicroseconds - 1000,
        isInQuery: true },

      // if lastModifieds and titles are equal, should fall back to bookmark
      // index
      { isBookmark: true,
        uri: "http://example.com/b3",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: 4,
        title: "y3",
        lastModified: timeInMicroseconds - 1000,
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[3],
      this._unsortedData[4],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
    query.onlyBookmarked = true;

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_LASTMODIFIED_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_TAGS_ASCENDING,

  setup: function() {
    LOG("Sorting test 9: SORT BY TAGS");

    this._unsortedData = [
      { isBookmark: true,
        uri: "http://url2.com/",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title x",
        isTag: true,
        tagArray: ["x", "y", "z"],
        isInQuery: true },

      { isBookmark: true,
        uri: "http://url1a.com/",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title y1",
        isTag: true,
        tagArray: ["a", "b"],
        isInQuery: true },

      { isBookmark: true,
        uri: "http://url3a.com/",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title w1",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://url0.com/",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title z",
        isTag: true,
        tagArray: ["a", "y", "z"],
        isInQuery: true },

      // if tags are equal, should fall back to title
      { isBookmark: true,
        uri: "http://url1b.com/",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title y2",
        isTag: true,
        tagArray: ["b", "a"],
        isInQuery: true },

      // if tags are equal, should fall back to title
      { isBookmark: true,
        uri: "http://url3b.com/",
        parentFolder: PlacesUtils.bookmarks.toolbarFolder,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title w2",
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[2],
      this._unsortedData[5],
      this._unsortedData[1],
      this._unsortedData[4],
      this._unsortedData[3],
      this._unsortedData[0],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
    query.onlyBookmarked = true;

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_TAGS_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////
// SORT_BY_ANNOTATION_* (int32)

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_ASCENDING,

  setup: function() {
    LOG("Sorting test 10: SORT BY ANNOTATION (int32)");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isVisit: true,
        isDetails: true,
        lastVisit: timeInMicroseconds,
        uri: "http://example.com/b1",
        title: "y1",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: 2,
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        lastVisit: timeInMicroseconds,
        uri: "http://example.com/a",
        title: "z",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: 1,
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        lastVisit: timeInMicroseconds,
        uri: "http://example.com/c",
        title: "x",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: 3,
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },

      // if annotations are equal, should fall back to title
      { isVisit: true,
        isDetails: true,
        lastVisit: timeInMicroseconds,
        uri: "http://example.com/b2",
        title: "y2",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: 2,
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[3],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingAnnotation = "sorting";
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////
// SORT_BY_ANNOTATION_* (int64)

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_ASCENDING,

  setup: function() {
    LOG("Sorting test 11: SORT BY ANNOTATION (int64)");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: timeInMicroseconds,
        title: "I",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: 0xffffffff1,
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://is.com/",
        lastVisit: timeInMicroseconds,
        title: "love",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: 0xffffffff0,
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://best.com/",
        lastVisit: timeInMicroseconds,
        title: "moz",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: 0xffffffff2,
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingAnnotation = "sorting";
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////
// SORT_BY_ANNOTATION_* (string)

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_ASCENDING,

  setup: function() {
    LOG("Sorting test 12: SORT BY ANNOTATION (string)");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: timeInMicroseconds,
        title: "I",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: "a",
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://is.com/",
        lastVisit: timeInMicroseconds,
        title: "love",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: "",
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://best.com/",
        lastVisit: timeInMicroseconds,
        title: "moz",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: "z",
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingAnnotation = "sorting";
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////
// SORT_BY_ANNOTATION_* (double)

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_ASCENDING,

  setup: function() {
    LOG("Sorting test 13: SORT BY ANNOTATION (double)");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: timeInMicroseconds,
        title: "I",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: 1.2,
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://is.com/",
        lastVisit: timeInMicroseconds,
        title: "love",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: 1.1,
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://best.com/",
        lastVisit: timeInMicroseconds,
        title: "moz",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: 1.3,
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = PlacesUtils.history.getNewQuery();

    // query options
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingAnnotation = "sorting";
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = PlacesUtils.history.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_ANNOTATION_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////
// SORT_BY_FRECENCY_*

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_FRECENCY_ASCENDING,

  setup: function() {
    LOG("Sorting test 13: SORT BY FRECENCY ");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: timeInMicroseconds++,
        title: "I",
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: timeInMicroseconds++,
        title: "I",
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: timeInMicroseconds++,
        title: "I",
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://is.com/",
        lastVisit: timeInMicroseconds++,
        title: "love",
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://best.com/",
        lastVisit: timeInMicroseconds++,
        title: "moz",
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://best.com/",
        lastVisit: timeInMicroseconds++,
        title: "moz",
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[3],
      this._unsortedData[5],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    yield task_populateDB(this._unsortedData);
  },

  check: function() {
    var query = PlacesUtils.history.getNewQuery();
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    var root = PlacesUtils.history.executeQuery(query, options).root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_FRECENCY_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

function run_test()
{
  run_next_test();
}

add_task(function test_sorting()
{
  for (let [, test] in Iterator(tests)) {
    yield test.setup();
    yield PlacesTestUtils.promiseAsyncUpdates();
    test.check();
    // sorting reversed, usually SORT_BY have ASC and DESC
    test.check_reverse();
    // Execute cleanup tasks
    remove_all_bookmarks();
    yield PlacesTestUtils.clearHistory();
  }
});
