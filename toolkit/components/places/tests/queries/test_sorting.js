/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var tests = [];

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_NONE,

  async setup() {
    info("Sorting test 1: SORT BY NONE");

    this._unsortedData = [
      {
        isBookmark: true,
        uri: "http://example.com/b",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "y",
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/a",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "z",
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/c",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "x",
        isInQuery: true,
      },
    ];

    this._sortedData = this._unsortedData;

    // This function in head_queries.js creates our database with the above data
    await task_populateDB(this._unsortedData);
  },

  check() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setParents([PlacesUtils.bookmarks.toolbarGuid]);

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

  check_reverse() {
    // no reverse sorting for SORT BY NONE
  },
});

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING,

  async setup() {
    info("Sorting test 2: SORT BY TITLE");

    this._unsortedData = [
      {
        isBookmark: true,
        uri: "http://example.com/b1",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "y",
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/a",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "z",
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/c",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "x",
        isInQuery: true,
      },

      // if titles are equal, should fall back to URI
      {
        isBookmark: true,
        uri: "http://example.com/b2",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "y",
        isInQuery: true,
      },
    ];

    this._sortedData = [
      this._unsortedData[2],
      this._unsortedData[0],
      this._unsortedData[3],
      this._unsortedData[1],
    ];

    // This function in head_queries.js creates our database with the above data
    await task_populateDB(this._unsortedData);
  },

  check() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setParents([PlacesUtils.bookmarks.toolbarGuid]);

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

  check_reverse() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_DESCENDING;
    this._sortedData.reverse();
    this.check();
  },
});

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_ASCENDING,

  async setup() {
    info("Sorting test 3: SORT BY DATE");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      {
        isVisit: true,
        isDetails: true,
        isBookmark: true,
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 0,
        uri: "http://example.com/c1",
        lastVisit: timeInMicroseconds - 2000,
        title: "x1",
        isInQuery: true,
      },

      {
        isVisit: true,
        isDetails: true,
        isBookmark: true,
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 1,
        uri: "http://example.com/a",
        lastVisit: timeInMicroseconds - 1000,
        title: "z",
        isInQuery: true,
      },

      {
        isVisit: true,
        isDetails: true,
        isBookmark: true,
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 2,
        uri: "http://example.com/b",
        lastVisit: timeInMicroseconds - 3000,
        title: "y",
        isInQuery: true,
      },

      // if dates are equal, should fall back to title
      {
        isVisit: true,
        isDetails: true,
        isBookmark: true,
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 3,
        uri: "http://example.com/c2",
        lastVisit: timeInMicroseconds - 2000,
        title: "x2",
        isInQuery: true,
      },

      // if dates and title are equal, should fall back to bookmark index
      {
        isVisit: true,
        isDetails: true,
        isBookmark: true,
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 4,
        uri: "http://example.com/c2",
        lastVisit: timeInMicroseconds - 2000,
        title: "x2",
        isInQuery: true,
      },
    ];

    this._sortedData = [
      this._unsortedData[2],
      this._unsortedData[0],
      this._unsortedData[3],
      this._unsortedData[4],
      this._unsortedData[1],
    ];

    // This function in head_queries.js creates our database with the above data
    await task_populateDB(this._unsortedData);
  },

  check() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setParents([PlacesUtils.bookmarks.toolbarGuid]);

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

  check_reverse() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;
    this._sortedData.reverse();
    this.check();
  },
});

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_URI_ASCENDING,

  async setup() {
    info("Sorting test 4: SORT BY URI");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      {
        isBookmark: true,
        isDetails: true,
        lastVisit: timeInMicroseconds,
        uri: "http://example.com/b",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 0,
        title: "y",
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/c",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 1,
        title: "x",
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/a",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 2,
        title: "z",
        isInQuery: true,
      },

      // if URIs are equal, should fall back to date
      {
        isBookmark: true,
        isDetails: true,
        lastVisit: timeInMicroseconds + 1000,
        uri: "http://example.com/c",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 3,
        title: "x",
        isInQuery: true,
      },

      // if no URI (e.g., node is a folder), should fall back to title
      {
        isFolder: true,
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 4,
        title: "y",
        isInQuery: true,
      },

      // if URIs and dates are equal, should fall back to bookmark index
      {
        isBookmark: true,
        isDetails: true,
        lastVisit: timeInMicroseconds + 1000,
        uri: "http://example.com/c",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 5,
        title: "x",
        isInQuery: true,
      },

      // if no URI and titles are equal, should fall back to bookmark index
      {
        isFolder: true,
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 6,
        title: "y",
        isInQuery: true,
      },

      // if no URI and titles are equal, should fall back to title
      {
        isFolder: true,
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 7,
        title: "z",
        isInQuery: true,
      },

      // Separator should go after folders.
      {
        isSeparator: true,
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 8,
        isInQuery: true,
      },
    ];

    this._sortedData = [
      this._unsortedData[4],
      this._unsortedData[6],
      this._unsortedData[7],
      this._unsortedData[8],
      this._unsortedData[2],
      this._unsortedData[0],
      this._unsortedData[1],
      this._unsortedData[3],
      this._unsortedData[5],
    ];

    // This function in head_queries.js creates our database with the above data
    await task_populateDB(this._unsortedData);
  },

  check() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setParents([PlacesUtils.bookmarks.toolbarGuid]);

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

  check_reverse() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_URI_DESCENDING;
    this._sortedData.reverse();
    this.check();
  },
});

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_ASCENDING,

  async setup() {
    info("Sorting test 5: SORT BY VISITCOUNT");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      {
        isBookmark: true,
        uri: "http://example.com/a",
        lastVisit: timeInMicroseconds,
        title: "z",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 0,
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/c",
        lastVisit: timeInMicroseconds,
        title: "x",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 1,
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/b1",
        lastVisit: timeInMicroseconds,
        title: "y1",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 2,
        isInQuery: true,
      },

      // if visitCounts are equal, should fall back to date
      {
        isBookmark: true,
        uri: "http://example.com/b2",
        lastVisit: timeInMicroseconds + 1000,
        title: "y2a",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 3,
        isInQuery: true,
      },

      // if visitCounts and dates are equal, should fall back to bookmark index
      {
        isBookmark: true,
        uri: "http://example.com/b2",
        lastVisit: timeInMicroseconds + 1000,
        title: "y2b",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 4,
        isInQuery: true,
      },
    ];

    this._sortedData = [
      this._unsortedData[0],
      this._unsortedData[2],
      this._unsortedData[3],
      this._unsortedData[4],
      this._unsortedData[1],
    ];

    // This function in head_queries.js creates our database with the above data
    await task_populateDB(this._unsortedData);
    // add visits to increase visit count
    await PlacesTestUtils.addVisits([
      {
        uri: uri("http://example.com/a"),
        transition: TRANSITION_TYPED,
        visitDate: timeInMicroseconds,
      },
      {
        uri: uri("http://example.com/b1"),
        transition: TRANSITION_TYPED,
        visitDate: timeInMicroseconds,
      },
      {
        uri: uri("http://example.com/b1"),
        transition: TRANSITION_TYPED,
        visitDate: timeInMicroseconds,
      },
      {
        uri: uri("http://example.com/b2"),
        transition: TRANSITION_TYPED,
        visitDate: timeInMicroseconds + 1000,
      },
      {
        uri: uri("http://example.com/b2"),
        transition: TRANSITION_TYPED,
        visitDate: timeInMicroseconds + 1000,
      },
      {
        uri: uri("http://example.com/c"),
        transition: TRANSITION_TYPED,
        visitDate: timeInMicroseconds,
      },
      {
        uri: uri("http://example.com/c"),
        transition: TRANSITION_TYPED,
        visitDate: timeInMicroseconds,
      },
      {
        uri: uri("http://example.com/c"),
        transition: TRANSITION_TYPED,
        visitDate: timeInMicroseconds,
      },
    ]);
  },

  check() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setParents([PlacesUtils.bookmarks.toolbarGuid]);

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

  check_reverse() {
    this._sortingMode =
      Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING;
    this._sortedData.reverse();
    this.check();
  },
});

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_ASCENDING,

  async setup() {
    info("Sorting test 7: SORT BY DATEADDED");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      {
        isBookmark: true,
        uri: "http://example.com/b1",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 0,
        title: "y1",
        dateAdded: timeInMicroseconds - 1000,
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/a",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 1,
        title: "z",
        dateAdded: timeInMicroseconds - 2000,
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/c",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 2,
        title: "x",
        dateAdded: timeInMicroseconds,
        isInQuery: true,
      },

      // if dateAddeds are equal, should fall back to title
      {
        isBookmark: true,
        uri: "http://example.com/b2",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 3,
        title: "y2",
        dateAdded: timeInMicroseconds - 1000,
        isInQuery: true,
      },

      // if dateAddeds and titles are equal, should fall back to bookmark index
      {
        isBookmark: true,
        uri: "http://example.com/b3",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 4,
        title: "y3",
        dateAdded: timeInMicroseconds - 1000,
        isInQuery: true,
      },
    ];

    this._sortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[3],
      this._unsortedData[4],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    await task_populateDB(this._unsortedData);
  },

  check() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setParents([PlacesUtils.bookmarks.toolbarGuid]);

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

  check_reverse() {
    this._sortingMode =
      Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING;
    this._sortedData.reverse();
    this.check();
  },
});

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_LASTMODIFIED_ASCENDING,

  async setup() {
    info("Sorting test 8: SORT BY LASTMODIFIED");

    var timeInMicroseconds = Date.now() * 1000;
    var timeAddedInMicroseconds = timeInMicroseconds - 10000;

    this._unsortedData = [
      {
        isBookmark: true,
        uri: "http://example.com/b1",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 0,
        title: "y1",
        dateAdded: timeAddedInMicroseconds,
        lastModified: timeInMicroseconds - 1000,
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/a",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 1,
        title: "z",
        dateAdded: timeAddedInMicroseconds,
        lastModified: timeInMicroseconds - 2000,
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://example.com/c",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 2,
        title: "x",
        dateAdded: timeAddedInMicroseconds,
        lastModified: timeInMicroseconds,
        isInQuery: true,
      },

      // if lastModifieds are equal, should fall back to title
      {
        isBookmark: true,
        uri: "http://example.com/b2",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 3,
        title: "y2",
        dateAdded: timeAddedInMicroseconds,
        lastModified: timeInMicroseconds - 1000,
        isInQuery: true,
      },

      // if lastModifieds and titles are equal, should fall back to bookmark
      // index
      {
        isBookmark: true,
        uri: "http://example.com/b3",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 4,
        title: "y3",
        dateAdded: timeAddedInMicroseconds,
        lastModified: timeInMicroseconds - 1000,
        isInQuery: true,
      },
    ];

    this._sortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[3],
      this._unsortedData[4],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    await task_populateDB(this._unsortedData);
  },

  check() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setParents([PlacesUtils.bookmarks.toolbarGuid]);

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

  check_reverse() {
    this._sortingMode =
      Ci.nsINavHistoryQueryOptions.SORT_BY_LASTMODIFIED_DESCENDING;
    this._sortedData.reverse();
    this.check();
  },
});

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_TAGS_ASCENDING,

  async setup() {
    info("Sorting test 9: SORT BY TAGS");

    this._unsortedData = [
      {
        isBookmark: true,
        uri: "http://url2.com/",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title x",
        isTag: true,
        tagArray: ["x", "y", "z"],
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://url1a.com/",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title y1",
        isTag: true,
        tagArray: ["a", "b"],
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://url3a.com/",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title w1",
        isInQuery: true,
      },

      {
        isBookmark: true,
        uri: "http://url0.com/",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title z",
        isTag: true,
        tagArray: ["a", "y", "z"],
        isInQuery: true,
      },

      // if tags are equal, should fall back to title
      {
        isBookmark: true,
        uri: "http://url1b.com/",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title y2",
        isTag: true,
        tagArray: ["b", "a"],
        isInQuery: true,
      },

      // if tags are equal, should fall back to title
      {
        isBookmark: true,
        uri: "http://url3b.com/",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        title: "title w2",
        isInQuery: true,
      },
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
    await task_populateDB(this._unsortedData);
  },

  check() {
    // Query
    var query = PlacesUtils.history.getNewQuery();
    query.setParents([PlacesUtils.bookmarks.toolbarGuid]);

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

  check_reverse() {
    this._sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_TAGS_DESCENDING;
    this._sortedData.reverse();
    this.check();
  },
});

// SORT_BY_FRECENCY_*

tests.push({
  _sortingMode: Ci.nsINavHistoryQueryOptions.SORT_BY_FRECENCY_ASCENDING,

  async setup() {
    info("Sorting test 13: SORT BY FRECENCY ");

    let timeInMicroseconds = PlacesUtils.toPRTime(Date.now() - 10000);

    function newTimeInMicroseconds() {
      timeInMicroseconds = timeInMicroseconds + 1000;
      return timeInMicroseconds;
    }

    this._unsortedData = [
      {
        isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: newTimeInMicroseconds(),
        title: "I",
        isInQuery: true,
      },

      {
        isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: newTimeInMicroseconds(),
        title: "I",
        isInQuery: true,
      },

      {
        isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: newTimeInMicroseconds(),
        title: "I",
        isInQuery: true,
      },

      {
        isVisit: true,
        isDetails: true,
        uri: "http://is.com/",
        lastVisit: newTimeInMicroseconds(),
        title: "love",
        isInQuery: true,
      },

      {
        isVisit: true,
        isDetails: true,
        uri: "http://best.com/",
        lastVisit: newTimeInMicroseconds(),
        title: "moz",
        isInQuery: true,
      },

      {
        isVisit: true,
        isDetails: true,
        uri: "http://best.com/",
        lastVisit: newTimeInMicroseconds(),
        title: "moz",
        isInQuery: true,
      },
    ];

    this._sortedData = [
      this._unsortedData[3],
      this._unsortedData[5],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    await task_populateDB(this._unsortedData);
  },

  check() {
    var query = PlacesUtils.history.getNewQuery();
    var options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    var root = PlacesUtils.history.executeQuery(query, options).root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse() {
    this._sortingMode =
      Ci.nsINavHistoryQueryOptions.SORT_BY_FRECENCY_DESCENDING;
    this._sortedData.reverse();
    this.check();
  },
});

add_task(async function test_sorting() {
  for (let test of tests) {
    await test.setup();
    await PlacesTestUtils.promiseAsyncUpdates();
    test.check();
    // sorting reversed, usually SORT_BY have ASC and DESC
    test.check_reverse();
    // Execute cleanup tasks
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  }
});
