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
               "phase3": "profile2",
               "phase4": "profile1"};

/*
 * Bookmark lists
 */
var bookmarks_initial = {
  "menu": [
    { uri: "http://www.google.com",
      title: "Google",
      changes: {
        title: "google"
      }
    },
    { uri: "http://bugzilla.mozilla.org/show_bug.cgi?id=%s",
      title: "Bugzilla"
    },
    { uri: "http://www.mozilla.com"
    },
    { uri: "http://www.cnn.com",
      description: "This is a description of the site a at www.cnn.com",
      changes: {
        description: "Global news"
      }
    }
  ]
};

var bookmarks_after = {
  "menu": [
    { uri: "http://www.google.com",
      title: "google"
    },
    { uri: "http://bugzilla.mozilla.org/show_bug.cgi?id=%s",
      title: "Bugzilla"
    },
    { uri: "http://www.mozilla.com"
    },
    { uri: "http://www.cnn.com",
      description: "Global news"
    }
  ]
};

/*
 * Test phases
 */

Phase("phase1", [
  [Bookmarks.add, bookmarks_initial],
  [Bookmarks.verify, bookmarks_initial],
  [Sync]
]);

Phase("phase2", [
  [Bookmarks.add, bookmarks_initial],
  [Bookmarks.verify, bookmarks_initial],
  [Sync]
]);

Phase("phase3", [
  [Bookmarks.verify, bookmarks_initial],
  [Bookmarks.modify, bookmarks_initial],
  [Bookmarks.verify, bookmarks_after],
  [Sync]
]);

Phase("phase4", [
  [Sync],
  [Bookmarks.verify, bookmarks_after]
]);

