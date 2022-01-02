/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

EnableEngines(["bookmarks"]);

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in JSON format as it will get parsed by the Python
 * testrunner. It is parsed by the YAML package, so it relatively flexible.
 */
var phases = { phase1: "profile1", phase2: "profile2" };

// the initial list of bookmarks to add to the browser
var bookmarks_initial = {
  menu: [{ folder: "testfolder", description: "it's just me, a test folder" }],
  "menu/testfolder": [{ uri: "http://www.mozilla.com", title: "Mozilla" }],
};

/*
 * Test phases
 */

// Add a bookmark folder which has a description, and sync.
Phase("phase1", [
  [Bookmarks.add, bookmarks_initial],
  [Bookmarks.verify, bookmarks_initial],
  [Sync],
]);

// Sync to profile2 and verify that the bookmark folder is created, along
// with its description.
Phase("phase2", [[Sync], [Bookmarks.verify, bookmarks_initial]]);
