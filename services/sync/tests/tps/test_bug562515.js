/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

EnableEngines(["bookmarks"]);

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in JSON format as it will get parsed by the Python
 * testrunner. It is parsed by the YAML package, so it relatively flexible.
 */
var phases = {
  phase1: "profile1",
  phase2: "profile2",
  phase3: "profile1",
  phase4: "profile2",
};

/*
 * Bookmark lists
 */

// the initial list of bookmarks to add to the browser
var bookmarks_initial = {
  menu: [
    {
      uri: "http://www.google.com",
      tags: ["google", "computers", "internet", "www"],
    },
    {
      uri: "http://bugzilla.mozilla.org/show_bug.cgi?id=%s",
      title: "Bugzilla",
      keyword: "bz",
    },
    { folder: "foldera" },
    { uri: "http://www.mozilla.com" },
    { separator: true },
    { folder: "folderb" },
  ],
  "menu/foldera": [
    { uri: "http://www.yahoo.com", title: "testing Yahoo" },
    {
      uri: "http://www.cnn.com",
      description: "This is a description of the site a at www.cnn.com",
    },
  ],
  "menu/folderb": [{ uri: "http://www.apple.com", tags: ["apple", "mac"] }],
  toolbar: [
    {
      uri: "place:queryType=0&sort=8&maxResults=10&beginTimeRef=1&beginTime=0",
      title: "Visited Today",
    },
  ],
};

// a list of bookmarks to delete during a 'delete' action
var bookmarks_to_delete = {
  menu: [
    {
      uri: "http://www.google.com",
      tags: ["google", "computers", "internet", "www"],
    },
  ],
  "menu/foldera": [{ uri: "http://www.yahoo.com", title: "testing Yahoo" }],
};

/*
 * Test phases
 */

// add bookmarks to profile1 and sync
Phase("phase1", [
  [Bookmarks.add, bookmarks_initial],
  [Bookmarks.verify, bookmarks_initial],
  [Sync],
]);

// sync to profile2 and verify that the bookmarks are present
Phase("phase2", [[Sync], [Bookmarks.verify, bookmarks_initial]]);

// delete some bookmarks from profile1, then sync with "wipe-client"
// set; finally, verify that the deleted bookmarks were restored.
Phase("phase3", [
  [Bookmarks.delete, bookmarks_to_delete],
  [Bookmarks.verifyNot, bookmarks_to_delete],
  [Sync, SYNC_WIPE_CLIENT],
  [Bookmarks.verify, bookmarks_initial],
]);

// sync profile2 again, verify no bookmarks have been deleted
Phase("phase4", [[Sync], [Bookmarks.verify, bookmarks_initial]]);
