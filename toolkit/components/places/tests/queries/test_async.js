/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var tests = [
  {
    desc: "nsNavHistoryFolderResultNode: Basic test, asynchronously open and " +
          "close container with a single child",

    loading: function(node, newState, oldState) {
      this.checkStateChanged("loading", 1);
      this.checkArgs("loading", node, oldState, node.STATE_CLOSED);
    },

    opened: function(node, newState, oldState) {
      this.checkStateChanged("opened", 1);
      this.checkState("loading", 1);
      this.checkArgs("opened", node, oldState, node.STATE_LOADING);

      print("Checking node children");
      compareArrayToResult(this.data, node);

      print("Closing container");
      node.containerOpen = false;
    },

    closed: function(node, newState, oldState) {
      this.checkStateChanged("closed", 1);
      this.checkState("opened", 1);
      this.checkArgs("closed", node, oldState, node.STATE_OPENED);
      this.success();
    }
  },

  {
    desc: "nsNavHistoryFolderResultNode: After async open and no changes, " +
          "second open should be synchronous",

    loading: function(node, newState, oldState) {
      this.checkStateChanged("loading", 1);
      this.checkState("closed", 0);
      this.checkArgs("loading", node, oldState, node.STATE_CLOSED);
    },

    opened: function(node, newState, oldState) {
      let cnt = this.checkStateChanged("opened", 1, 2);
      let expectOldState = cnt === 1 ? node.STATE_LOADING : node.STATE_CLOSED;
      this.checkArgs("opened", node, oldState, expectOldState);

      print("Checking node children");
      compareArrayToResult(this.data, node);

      print("Closing container");
      node.containerOpen = false;
    },

    closed: function(node, newState, oldState) {
      let cnt = this.checkStateChanged("closed", 1, 2);
      this.checkArgs("closed", node, oldState, node.STATE_OPENED);

      switch (cnt) {
      case 1:
        node.containerOpen = true;
        break;
      case 2:
        this.success();
        break;
      }
    }
  },

  {
    desc: "nsNavHistoryFolderResultNode: After closing container in " +
          "loading(), opened() should not be called",

    loading: function(node, newState, oldState) {
      this.checkStateChanged("loading", 1);
      this.checkArgs("loading", node, oldState, node.STATE_CLOSED);
      print("Closing container");
      node.containerOpen = false;
    },

    opened: function(node, newState, oldState) {
      do_throw("opened should not be called");
    },

    closed: function(node, newState, oldState) {
      this.checkStateChanged("closed", 1);
      this.checkState("loading", 1);
      this.checkArgs("closed", node, oldState, node.STATE_LOADING);
      this.success();
    }
  }
];


/**
 * Instances of this class become the prototypes of the test objects above.
 * Each test can therefore use the methods of this class, or they can override
 * them if they want.  To run a test, call setup() and then run().
 */
function Test() {
  // This maps a state name to the number of times it's been observed.
  this.stateCounts = {};
  // Promise object resolved when the next test can be run.
  this.deferNextTest = Promise.defer();
}

Test.prototype = {
  /**
   * Call this when an observer observes a container state change to sanity
   * check the arguments.
   *
   * @param aNewState
   *        The name of the new state.  Used only for printing out helpful info.
   * @param aNode
   *        The node argument passed to containerStateChanged.
   * @param aOldState
   *        The old state argument passed to containerStateChanged.
   * @param aExpectOldState
   *        The expected old state.
   */
  checkArgs: function(aNewState, aNode, aOldState, aExpectOldState) {
    print("Node passed on " + aNewState + " should be result.root");
    do_check_eq(this.result.root, aNode);
    print("Old state passed on " + aNewState + " should be " + aExpectOldState);

    // aOldState comes from xpconnect and will therefore be defined.  It may be
    // zero, though, so use strict equality just to make sure aExpectOldState is
    // also defined.
    do_check_true(aOldState === aExpectOldState);
  },

  /**
   * Call this when an observer observes a container state change.  It registers
   * the state change and ensures that it has been observed the given number
   * of times.  See checkState for parameter explanations.
   *
   * @return The number of times aState has been observed, including the new
   *         observation.
   */
  checkStateChanged: function(aState, aExpectedMin, aExpectedMax) {
    print(aState + " state change observed");
    if (!this.stateCounts.hasOwnProperty(aState))
      this.stateCounts[aState] = 0;
    this.stateCounts[aState]++;
    return this.checkState(aState, aExpectedMin, aExpectedMax);
  },

  /**
   * Ensures that the state has been observed the given number of times.
   *
   * @param  aState
   *         The name of the state.
   * @param  aExpectedMin
   *         The state must have been observed at least this number of times.
   * @param  aExpectedMax
   *         The state must have been observed at most this number of times.
   *         This parameter is optional.  If undefined, it's set to
   *         aExpectedMin.
   * @return The number of times aState has been observed, including the new
   *         observation.
   */
  checkState: function(aState, aExpectedMin, aExpectedMax) {
    let cnt = this.stateCounts[aState] || 0;
    if (aExpectedMax === undefined)
      aExpectedMax = aExpectedMin;
    if (aExpectedMin === aExpectedMax) {
      print(aState + " should be observed only " + aExpectedMin +
            " times (actual = " + cnt + ")");
    }
    else {
      print(aState + " should be observed at least " + aExpectedMin +
            " times and at most " + aExpectedMax + " times (actual = " +
            cnt + ")");
    }
    do_check_true(cnt >= aExpectedMin && cnt <= aExpectedMax);
    return cnt;
  },

  /**
   * Asynchronously opens the root of the test's result.
   */
  openContainer: function() {
    // Set up the result observer.  It delegates to this object's callbacks and
    // wraps them in a try-catch so that errors don't get eaten.
    let self = this;
    this.observer = {
      containerStateChanged: function(container, oldState, newState) {
        print("New state passed to containerStateChanged() should equal the " +
              "container's current state");
        do_check_eq(newState, container.state);

        try {
          switch (newState) {
          case Ci.nsINavHistoryContainerResultNode.STATE_LOADING:
            self.loading(container, newState, oldState);
            break;
          case Ci.nsINavHistoryContainerResultNode.STATE_OPENED:
            self.opened(container, newState, oldState);
            break;
          case Ci.nsINavHistoryContainerResultNode.STATE_CLOSED:
            self.closed(container, newState, oldState);
            break;
          default:
            do_throw("Unexpected new state! " + newState);
          }
        }
        catch (err) {
          do_throw(err);
        }
      },
    };
    this.result.addObserver(this.observer, false);

    print("Opening container");
    this.result.root.containerOpen = true;
  },

  /**
   * Starts the test and returns a promise resolved when the test completes.
   */
  run: function() {
    this.openContainer();
    return this.deferNextTest.promise;
  },

  /**
   * This must be called before run().  It adds a bookmark and sets up the
   * test's result.  Override if need be.
   */
  setup: function*() {
    // Populate the database with different types of bookmark items.
    this.data = DataHelper.makeDataArray([
      { type: "bookmark" },
      { type: "separator" },
      { type: "folder" },
      { type: "bookmark", uri: "place:terms=foo" }
    ]);
    yield task_populateDB(this.data);

    // Make a query.
    this.query = PlacesUtils.history.getNewQuery();
    this.query.setFolders([DataHelper.defaults.bookmark.parent], 1);
    this.opts = PlacesUtils.history.getNewQueryOptions();
    this.opts.asyncEnabled = true;
    this.result = PlacesUtils.history.executeQuery(this.query, this.opts);
  },

  /**
   * Call this when the test has succeeded.  It cleans up resources and starts
   * the next test.
   */
  success: function() {
    this.result.removeObserver(this.observer);

    // Resolve the promise object that indicates that the next test can be run.
    this.deferNextTest.resolve();
  }
};

/**
 * This makes it a little bit easier to use the functions of head_queries.js.
 */
var DataHelper = {
  defaults: {
    bookmark: {
      parent: PlacesUtils.bookmarks.unfiledBookmarksFolder,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      uri: "http://example.com/",
      title: "test bookmark"
    },

    folder: {
      parent: PlacesUtils.bookmarks.unfiledBookmarksFolder,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      title: "test folder"
    },

    separator: {
      parent: PlacesUtils.bookmarks.unfiledBookmarksFolder,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    }
  },

  /**
   * Converts an array of simple bookmark item descriptions to the more verbose
   * format required by task_populateDB() in head_queries.js.
   *
   * @param  aData
   *         An array of objects, each of which describes a bookmark item.
   * @return An array of objects suitable for passing to populateDB().
   */
  makeDataArray: function DH_makeDataArray(aData) {
    let self = this;
    return aData.map(function(dat) {
      let type = dat.type;
      dat = self._makeDataWithDefaults(dat, self.defaults[type]);
      switch (type) {
      case "bookmark":
        return {
          isBookmark: true,
          uri: dat.uri,
          parentGuid: dat.parentGuid,
          index: PlacesUtils.bookmarks.DEFAULT_INDEX,
          title: dat.title,
          isInQuery: true
        };
      case "separator":
        return {
          isSeparator: true,
          parentGuid: dat.parentGuid,
          index: PlacesUtils.bookmarks.DEFAULT_INDEX,
          isInQuery: true
        };
      case "folder":
        return {
          isFolder: true,
          parentGuid: dat.parentGuid,
          index: PlacesUtils.bookmarks.DEFAULT_INDEX,
          title: dat.title,
          isInQuery: true
        };
      default:
        do_throw("Unknown data type when populating DB: " + type);
        return undefined;
      }
    });
  },

  /**
   * Returns a copy of aData, except that any properties that are undefined but
   * defined in aDefaults are set to the corresponding values in aDefaults.
   *
   * @param  aData
   *         An object describing a bookmark item.
   * @param  aDefaults
   *         An object describing the default bookmark item.
   * @return A copy of aData with defaults values set.
   */
  _makeDataWithDefaults: function DH__makeDataWithDefaults(aData, aDefaults) {
    let dat = {};
    for (let [prop, val] of Object.entries(aDefaults)) {
      dat[prop] = aData.hasOwnProperty(prop) ? aData[prop] : val;
    }
    return dat;
  }
};

function run_test()
{
  run_next_test();
}

add_task(function* test_async()
{
  for (let test of tests) {
    yield PlacesUtils.bookmarks.eraseEverything();

    test.__proto__ = new Test();
    yield test.setup();

    print("------ Running test: " + test.desc);
    yield test.run();
  }

  yield PlacesUtils.bookmarks.eraseEverything();
  print("All tests done, exiting");
});
