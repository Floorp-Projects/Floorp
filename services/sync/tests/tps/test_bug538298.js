/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in strict JSON format, as it will get parsed by the Python
 * testrunner (no single quotes, extra comma's, etc).
 */
EnableEngines(["bookmarks"]);

var phases = { "phase1": "profile1",
               "phase2": "profile2",
               "phase3": "profile1",
               "phase4": "profile2" };

/*
 * Bookmark asset lists: these define bookmarks that are used during the test
 */

// the initial list of bookmarks to add to the browser
var bookmarks_initial = {
  "toolbar": [
    { uri: "http://www.google.com",
      title: "Google"
    },
    { uri: "http://www.cnn.com",
      title: "CNN",
      changes: {
        position: "Google"
      }
    },
    { uri: "http://www.mozilla.com",
      title: "Mozilla"
    },
    { uri: "http://www.firefox.com",
      title: "Firefox",
      changes: {
        position: "Mozilla"
      }
    }
  ]
};

var bookmarks_after_move = {
  "toolbar": [
    { uri: "http://www.cnn.com",
      title: "CNN"
    },
    { uri: "http://www.google.com",
      title: "Google"
    },
    { uri: "http://www.firefox.com",
      title: "Firefox"
    },
    { uri: "http://www.mozilla.com",
      title: "Mozilla"
    }
  ]
};

/*
 * Test phases
 */

// Add four bookmarks to the toolbar and sync.
Phase('phase1', [
  [Bookmarks.add, bookmarks_initial],
  [Bookmarks.verify, bookmarks_initial],
  [Sync]
]);

// Sync to profile2 and verify that all four bookmarks are present.
Phase('phase2', [
  [Sync],
  [Bookmarks.verify, bookmarks_initial]
]);

// Change the order of the toolbar bookmarks, and sync.
Phase('phase3', [
  [Sync],
  [Bookmarks.verify, bookmarks_initial],
  [Bookmarks.modify, bookmarks_initial],
  [Bookmarks.verify, bookmarks_after_move],
  [Sync],
]);

// Go back to profile2, sync, and verify that the bookmarks are reordered
// as expected.
Phase('phase4', [
  [Sync],
  [Bookmarks.verify, bookmarks_after_move]
]);

