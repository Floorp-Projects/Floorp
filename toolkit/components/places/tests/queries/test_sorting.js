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
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Marco Bonardo <mak77@bonardo.net>
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

var tests = [];

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: histsvc.SORT_BY_NONE,

  setup: function() {
    LOG("Sorting test 1: SORT BY NONE");

    this._unsortedData = [
      { isBookmark: true,
        uri: "http://urlB.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "titleB",
        keyword: "keywordB",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://urlA.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "titleA",
        keyword: "keywordA",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://urlC.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "titleC",
        keyword: "keywordC",
        isInQuery: true },
    ];

    this._sortedData = this._unsortedData;

    // This function in head_queries.js creates our database with the above data
    populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = histsvc.getNewQuery();
    query.setFolders([bmsvc.toolbarFolder], 1);
    query.onlyBookmarked = true;
    
    // query options
    var options = histsvc.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = histsvc.executeQuery(query, options);
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
  _sortingMode: histsvc.SORT_BY_TITLE_ASCENDING,

  setup: function() {
    LOG("Sorting test 2: SORT BY TITLE");

    this._unsortedData = [
      { isBookmark: true,
        uri: "http://urlB.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "titleB",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://urlA.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "titleA",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://urlC.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "titleC",
        isInQuery: true },
    ];

    this._sortedData = this._unsortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = histsvc.getNewQuery();
    query.setFolders([bmsvc.toolbarFolder], 1);
    query.onlyBookmarked = true;
    
    // query options
    var options = histsvc.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = histsvc.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = histsvc.SORT_BY_TITLE_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: histsvc.SORT_BY_DATE_ASCENDING,

  setup: function() {
    LOG("Sorting test 3: SORT BY DATE");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: timeInMicroseconds - 2,
        title: "I",
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://is.com/",
        lastVisit: timeInMicroseconds - 1,
        title: "love",
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://best.com/",
        lastVisit: timeInMicroseconds - 3,
        title: "moz",
        isInQuery: true },
    ];

    this._sortedData = this._unsortedData = [
      this._unsortedData[2],
      this._unsortedData[0],
      this._unsortedData[1],
    ];

    // This function in head_queries.js creates our database with the above data
    populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = histsvc.getNewQuery();

    // query options
    var options = histsvc.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = histsvc.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = histsvc.SORT_BY_DATE_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: histsvc.SORT_BY_URI_ASCENDING,

  setup: function() {
    LOG("Sorting test 4: SORT BY URI");

    this._unsortedData = [
      { isBookmark: true,
        uri: "http://is.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "I",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://moz.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "love",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://best.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "moz",
        isInQuery: true },
    ];

    this._sortedData = this._unsortedData = [
      this._unsortedData[2],
      this._unsortedData[0],
      this._unsortedData[1],
    ];

    // This function in head_queries.js creates our database with the above data
    populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = histsvc.getNewQuery();
    query.setFolders([bmsvc.toolbarFolder], 1);
    query.onlyBookmarked = true;
    
    // query options
    var options = histsvc.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = histsvc.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = histsvc.SORT_BY_URI_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: histsvc.SORT_BY_VISITCOUNT_ASCENDING,

  setup: function() {
    LOG("Sorting test 5: SORT BY VISITCOUNT");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: timeInMicroseconds,
        title: "I",
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://is.com/",
        lastVisit: timeInMicroseconds,
        title: "love",
        isInQuery: true },

      { isVisit: true,
        isDetails: true,
        uri: "http://best.com/",
        lastVisit: timeInMicroseconds,
        title: "moz",
        isInQuery: true },
    ];

    this._sortedData = this._unsortedData = [
      this._unsortedData[0],
      this._unsortedData[2],
      this._unsortedData[1],
    ];

    // This function in head_queries.js creates our database with the above data
    populateDB(this._unsortedData);
    // add visits to increase visit count
    histsvc.addVisit(uri("http://is.com/"), timeInMicroseconds, null,
                     histsvc.TRANSITION_TYPED, false, 0);
    histsvc.addVisit(uri("http://is.com/"), timeInMicroseconds, null,
                     histsvc.TRANSITION_TYPED, false, 0);
    histsvc.addVisit(uri("http://best.com/"), timeInMicroseconds, null,
                     histsvc.TRANSITION_TYPED, false, 0);                     
  },

  check: function() {
    // Query
    var query = histsvc.getNewQuery();

    // query options
    var options = histsvc.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = histsvc.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = histsvc.SORT_BY_VISITCOUNT_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: histsvc.SORT_BY_KEYWORD_ASCENDING,

  setup: function() {
    LOG("Sorting test 6: SORT BY KEYWORD");

    this._unsortedData = [
      { isBookmark: true,
        uri: "http://moz.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "I",
        keyword: "a",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://is.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "love",
        keyword: "c",
        isInQuery: true },

      { isBookmark: true,
        uri: "http://best.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "moz",
        keyword: "b",
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[0],
      this._unsortedData[2],
      this._unsortedData[1],
    ];

    // This function in head_queries.js creates our database with the above data
    populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = histsvc.getNewQuery();
    query.setFolders([bmsvc.toolbarFolder], 1);
    query.onlyBookmarked = true;
    
    // query options
    var options = histsvc.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = histsvc.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = histsvc.SORT_BY_KEYWORD_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: histsvc.SORT_BY_DATEADDED_ASCENDING,

  setup: function() {
    LOG("Sorting test 7: SORT BY DATEADDED");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isBookmark: true,
        uri: "http://moz.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "I",
        dateAdded: timeInMicroseconds -1,
        isInQuery: true },

      { isBookmark: true,
        uri: "http://is.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "love",
        dateAdded: timeInMicroseconds - 2,
        isInQuery: true },

      { isBookmark: true,
        uri: "http://best.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "moz",
        dateAdded: timeInMicroseconds,
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = histsvc.getNewQuery();
    query.setFolders([bmsvc.toolbarFolder], 1);
    query.onlyBookmarked = true;
    
    // query options
    var options = histsvc.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = histsvc.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = histsvc.SORT_BY_DATEADDED_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

tests.push({
  _sortingMode: histsvc.SORT_BY_LASTMODIFIED_ASCENDING,

  setup: function() {
    LOG("Sorting test 8: SORT BY LASTMODIFIED");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isBookmark: true,
        uri: "http://moz.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "I",
        lastModified: timeInMicroseconds -1,
        isInQuery: true },

      { isBookmark: true,
        uri: "http://is.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "love",
        lastModified: timeInMicroseconds - 2,
        isInQuery: true },

      { isBookmark: true,
        uri: "http://best.com/",
        parentFolder: bmsvc.toolbarFolder,
        index: bmsvc.DEFAULT_INDEX,
        title: "moz",
        lastModified: timeInMicroseconds,
        isInQuery: true },
    ];

    this._sortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    populateDB(this._unsortedData);
  },

  check: function() {
    // Query
    var query = histsvc.getNewQuery();
    query.setFolders([bmsvc.toolbarFolder], 1);
    query.onlyBookmarked = true;
    
    // query options
    var options = histsvc.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = histsvc.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = histsvc.SORT_BY_LASTMODIFIED_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////
// TEST 9
// SORT_BY_TAGS_ASCENDING
// SORT_BY_TAGS_DESCENDING
//XXX bug 444179

////////////////////////////////////////////////////////////////////////////////
// SORT_BY_ANNOTATION_ASCENDING = 19;

tests.push({
  _sortingMode: histsvc.SORT_BY_ANNOTATION_ASCENDING,

  setup: function() {
    LOG("Sorting test 10: SORT BY ANNOTATION");

    var timeInMicroseconds = Date.now() * 1000;
    this._unsortedData = [
      { isVisit: true,
        isDetails: true,
        uri: "http://moz.com/",
        lastVisit: timeInMicroseconds,
        title: "I",
        isPageAnnotation: true,
        annoName: "sorting",
        annoVal: 2,
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
        annoVal: 1,
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
        annoVal: 3,
        annoFlags: 0,
        annoExpiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
        isInQuery: true },
    ];

    this._sortedData = this._unsortedData = [
      this._unsortedData[1],
      this._unsortedData[0],
      this._unsortedData[2],
    ];

    // This function in head_queries.js creates our database with the above data
    populateDB(this._unsortedData);                  
  },

  check: function() {
    // Query
    var query = histsvc.getNewQuery();

    // query options
    var options = histsvc.getNewQueryOptions();
    options.sortingMode = this._sortingMode;

    // Results - this gets the result set and opens it for reading and modification.
    var result = histsvc.executeQuery(query, options);
    var root = result.root;
    root.containerOpen = true;
    compareArrayToResult(this._sortedData, root);
    root.containerOpen = false;
  },

  check_reverse: function() {
    this._sortingMode = histsvc.SORT_BY_ANNOTATION_DESCENDING;
    this._sortedData.reverse();
    this.check();
  }
});

////////////////////////////////////////////////////////////////////////////////

function prepare_for_next_test() {
  // Execute cleanup tasks
  bhistsvc.removeAllPages();
  remove_all_bookmarks();
}

/**
 * run_test is where the magic happens.  This is automatically run by the test
 * harness.  It is where you do the work of creating the query, running it, and
 * playing with the result set.
 */
function run_test() {
  prepare_for_next_test();
  while (tests.length) {
    let test = tests.shift();
    test.setup();
    test.check();
    // sorting reversed, usually SORT_BY have ASC and DESC
    test.check_reverse();
    prepare_for_next_test();
  }
}
