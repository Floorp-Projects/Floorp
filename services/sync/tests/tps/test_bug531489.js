/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

EnableEngines(["bookmarks"]);

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in JSON format as it will get parsed by the Python
 * testrunner. It is parsed by the YAML package, so it relatively flexible.
 */
var phases = { phase1: "profile1", phase2: "profile2", phase3: "profile1" };

/*
 * Bookmark asset lists: these define bookmarks that are used during the test
 */

// the initial list of bookmarks to add to the browser
var bookmarks_initial = {
  menu: [
    { folder: "foldera" },
    { uri: "http://www.google.com", title: "Google" },
  ],
  "menu/foldera": [{ uri: "http://www.google.com", title: "Google" }],
  toolbar: [{ uri: "http://www.google.com", title: "Google" }],
};

/*
 * Test phases
 */

// Add three bookmarks with the same url to different locations and sync.
Phase("phase1", [
  [Bookmarks.add, bookmarks_initial],
  [Bookmarks.verify, bookmarks_initial],
  [Sync],
]);

// Sync to profile2 and verify that all three bookmarks are present
Phase("phase2", [[Sync], [Bookmarks.verify, bookmarks_initial]]);

// Sync again to profile1 and verify that all three bookmarks are still
// present.
Phase("phase3", [[Sync], [Bookmarks.verify, bookmarks_initial]]);
