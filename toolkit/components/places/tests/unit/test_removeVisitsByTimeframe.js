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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
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

const bmsvc = PlacesUtils.bookmarks;
const histsvc = PlacesUtils.history;

const NOW = Date.now() * 1000;
const TEST_URL = "http://example.com/";
const TEST_URI = uri(TEST_URL);
const PLACE_URL = "place:queryType=0&sort=8&maxResults=10";
const PLACE_URI = uri(PLACE_URL);

var gTests = [
  {
    desc: "Remove some visits outside valid timeframe from an unbookmarked URI",
    run:   function () {
      print("Add 10 visits for the URI from way in the past.");
      for (let i = 0; i < 10; i++) {
        histsvc.addVisit(TEST_URI,
                         NOW - 1000 - i,
                         null,
                         histsvc.TRANSITION_TYPED,
                         false,
                         0);
      }
      waitForAsyncUpdates(this.continue_run, this);
    },
    continue_run: function () {
      print("Remove visits using timerange outside the URI's visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(TEST_URL));

      print("Run a history query and check that all visits still exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 10);
      for (let i = 0; i < resultRoot.childCount; i++) {
        var visitTime = resultRoot.getChild(i).time;
        do_check_eq(visitTime, NOW - 1000 - i);
      }
      resultRoot.containerOpen = false;

      print("nsIGlobalHistory2.isVisited should return true.");
      do_check_true(histsvc.QueryInterface(Ci.nsIGlobalHistory2).
                    isVisited(TEST_URI));

      waitForAsyncUpdates(function () {
        print("Frecency should be positive.")
        do_check_true(frecencyForUrl(TEST_URI) > 0);
        run_next_test();
      });
    }
  },

  {
    desc: "Remove some visits outside valid timeframe from a bookmarked URI",
    run:   function () {
      print("Add 10 visits for the URI from way in the past.");
      for (let i = 0; i < 10; i++) {
        histsvc.addVisit(TEST_URI,
                         NOW - 1000 - i,
                         null,
                         histsvc.TRANSITION_TYPED,
                         false,
                         0);
      }

      print("Bookmark the URI.");
      bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder,
                           TEST_URI,
                           bmsvc.DEFAULT_INDEX,
                           "bookmark title");

      waitForAsyncUpdates(this.continue_run, this);
    },
    continue_run: function () {
      print("Remove visits using timerange outside the URI's visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(TEST_URL));

      print("Run a history query and check that all visits still exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 10);
      for (let i = 0; i < resultRoot.childCount; i++) {
        var visitTime = resultRoot.getChild(i).time;
        do_check_eq(visitTime, NOW - 1000 - i);
      }
      resultRoot.containerOpen = false;

      print("nsIGlobalHistory2.isVisited should return true.");
      do_check_true(histsvc.QueryInterface(Ci.nsIGlobalHistory2).
                    isVisited(TEST_URI));

      waitForAsyncUpdates(function () {
        print("Frecency should be positive.")
        do_check_true(frecencyForUrl(TEST_URI) > 0);
        run_next_test();
      });
    }
  },

  {
    desc: "Remove some visits from an unbookmarked URI",
    run:   function () {
      print("Add 10 visits for the URI from now to 9 usecs in the past.");
      for (let i = 0; i < 10; i++) {
        histsvc.addVisit(TEST_URI,
                         NOW - i,
                         null,
                         histsvc.TRANSITION_TYPED,
                         false,
                         0);
      }
      waitForAsyncUpdates(this.continue_run, this);
    },
    continue_run: function () {
      print("Remove the 5 most recent visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 4, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(TEST_URL));

      print("Run a history query and check that only the older 5 visits " +
            "still exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 5);
      for (let i = 0; i < resultRoot.childCount; i++) {
        var visitTime = resultRoot.getChild(i).time;
        do_check_eq(visitTime, NOW - i - 5);
      }
      resultRoot.containerOpen = false;

      print("nsIGlobalHistory2.isVisited should return true.");
      do_check_true(histsvc.QueryInterface(Ci.nsIGlobalHistory2).
                    isVisited(TEST_URI));

      waitForAsyncUpdates(function () {
        print("Frecency should be positive.")
        do_check_true(frecencyForUrl(TEST_URI) > 0);
        run_next_test();
      });
    }
  },

  {
    desc: "Remove some visits from a bookmarked URI",
    run:   function () {
      print("Add 10 visits for the URI from now to 9 usecs in the past.");
      for (let i = 0; i < 10; i++) {
        histsvc.addVisit(TEST_URI,
                         NOW - i,
                         null,
                         histsvc.TRANSITION_TYPED,
                         false,
                         0);
      }

      print("Bookmark the URI.");
      bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder,
                           TEST_URI,
                           bmsvc.DEFAULT_INDEX,
                           "bookmark title");
      waitForAsyncUpdates(this.continue_run, this);
    },
    continue_run: function () {
      print("Remove the 5 most recent visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 4, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(TEST_URL));

      print("Run a history query and check that only the older 5 visits " +
            "still exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 5);
      for (let i = 0; i < resultRoot.childCount; i++) {
        var visitTime = resultRoot.getChild(i).time;
        do_check_eq(visitTime, NOW - i - 5);
      }
      resultRoot.containerOpen = false;

      print("nsIGlobalHistory2.isVisited should return true.");
      do_check_true(histsvc.QueryInterface(Ci.nsIGlobalHistory2).
                    isVisited(TEST_URI));

      waitForAsyncUpdates(function () {
        print("Frecency should be positive.")
        do_check_true(frecencyForUrl(TEST_URI) > 0);
        run_next_test();
      });
    }
  },

  {
    desc: "Remove all visits from an unbookmarked URI",
    run:   function () {
      print("Add some visits for the URI.");
      for (let i = 0; i < 10; i++) {
        histsvc.addVisit(TEST_URI,
                         NOW - i,
                         null,
                         histsvc.TRANSITION_TYPED,
                         false,
                         0);
      }
      waitForAsyncUpdates(this.continue_run, this);
    },
    continue_run: function () {
      print("Remove all visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      print("URI should no longer exist in moz_places.");
      do_check_false(page_in_database(TEST_URL));

      print("Run a history query and check that no visits exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 0);
      resultRoot.containerOpen = false;

      print("nsIGlobalHistory2.isVisited should return false.");
      do_check_false(histsvc.QueryInterface(Ci.nsIGlobalHistory2).
                       isVisited(TEST_URI));
      run_next_test();
    }
  },

  {
    desc: "Remove all visits from an unbookmarked place: URI",
    run:   function () {
      print("Add some visits for the URI.");
      for (let i = 0; i < 10; i++) {
        histsvc.addVisit(PLACE_URI,
                         NOW - i,
                         null,
                         histsvc.TRANSITION_TYPED,
                         false,
                         0);
      }
      waitForAsyncUpdates(this.continue_run, this);
    },
    continue_run: function () {
      print("Remove all visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(PLACE_URL));

      print("Run a history query and check that no visits exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 0);
      resultRoot.containerOpen = false;

      print("nsIGlobalHistory2.isVisited should return false.");
      do_check_false(histsvc.QueryInterface(Ci.nsIGlobalHistory2).
                       isVisited(PLACE_URI));

      waitForAsyncUpdates(function () {
        print("Frecency should be zero.")
        do_check_eq(frecencyForUrl(PLACE_URL), 0);
        run_next_test();
      });
    }
  },

  {
    desc: "Remove all visits from a bookmarked URI",
    run:   function () {
      print("Add some visits for the URI.");
      for (let i = 0; i < 10; i++) {
        histsvc.addVisit(TEST_URI,
                         NOW - i,
                         null,
                         histsvc.TRANSITION_TYPED,
                         false,
                         0);
      }

      print("Bookmark the URI.");
      bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder,
                           TEST_URI,
                           bmsvc.DEFAULT_INDEX,
                           "bookmark title");
      waitForAsyncUpdates(this.continue_run, this);
    },
    continue_run: function () {
      print("Remove all visits.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      print("URI should still exist in moz_places.");
      do_check_true(page_in_database(TEST_URL));

      print("Run a history query and check that no visits exist.");
      var query = histsvc.getNewQuery();
      var opts = histsvc.getNewQueryOptions();
      opts.resultType = opts.RESULTS_AS_VISIT;
      opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
      var resultRoot = histsvc.executeQuery(query, opts).root;
      resultRoot.containerOpen = true;
      do_check_eq(resultRoot.childCount, 0);
      resultRoot.containerOpen = false;

      print("nsIGlobalHistory2.isVisited should return false.");
      do_check_false(histsvc.QueryInterface(Ci.nsIGlobalHistory2).
                       isVisited(TEST_URI));

      print("nsINavBookmarksService.isBookmarked should return true.");
      do_check_true(bmsvc.isBookmarked(TEST_URI));

      waitForAsyncUpdates(function () {
        print("Frecency should be negative.")
        do_check_true(frecencyForUrl(TEST_URI) < 0);
        run_next_test();
      });
    }
  },

  {
    desc: "Remove some visits from a zero frecency URI retains zero frecency",
    run: function () {
      do_log_info("Add some visits for the URI.");
      addVisits([{ uri: TEST_URI, transition: TRANSITION_FRAMED_LINK,
                   visitDate: (NOW - 86400000000) },
                 { uri: TEST_URI, transition: TRANSITION_FRAMED_LINK,
                  visitDate: NOW }],
                this.continue_run.bind(this));
    },
    continue_run: function () {
      do_log_info("Remove newer visit.");
      histsvc.QueryInterface(Ci.nsIBrowserHistory).
        removeVisitsByTimeframe(NOW - 10, NOW);

      waitForAsyncUpdates(function() {
        do_log_info("URI should still exist in moz_places.");
        do_check_true(page_in_database(TEST_URL));
        do_log_info("Frecency should be zero.")
        do_check_eq(frecencyForUrl(TEST_URI), 0);
        run_next_test();
      });
    }
  }
];

///////////////////////////////////////////////////////////////////////////////

function run_test()
{
  do_test_pending();
  run_next_test();
}

function run_next_test() {
  if (gTests.length) {
    let test = gTests.shift();
    print("\n ***Test: " + test.desc);
    waitForClearHistory(function() {
      remove_all_bookmarks();
      DBConn().executeSimpleSQL("DELETE FROM moz_places");
      test.run.call(test);
    });
  }
  else {
    do_test_finished();
  }
}
