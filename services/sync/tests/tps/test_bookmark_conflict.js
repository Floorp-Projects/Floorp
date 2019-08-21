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

// the initial list of bookmarks to add to the browser
var bookmarksInitial = {
  menu: [
    { folder: "foldera" },
    { folder: "folderb" },
    { folder: "folderc" },
    { folder: "folderd" },
  ],

  "menu/foldera": [{ uri: "http://www.cnn.com", title: "CNN" }],
  "menu/folderb": [{ uri: "http://www.apple.com", title: "Apple", tags: [] }],
  "menu/folderc": [{ uri: "http://www.yahoo.com", title: "Yahoo" }],

  "menu/folderd": [],
};

// a list of bookmarks to delete during a 'delete' action on P2
var bookmarksToDelete = {
  menu: [{ folder: "foldera" }, { folder: "folderb" }],
  "menu/folderc": [{ uri: "http://www.yahoo.com", title: "Yahoo" }],
};

// the modifications to make on P1, after P2 has synced, but before P1 has gotten
// P2's changes
var bookmarkMods = {
  menu: [
    { folder: "foldera" },
    { folder: "folderb" },
    { folder: "folderc" },
    { folder: "folderd" },
  ],

  // we move this child out of its folder (p1), after deleting the folder (p2)
  // and expect the child to come back to p2 after sync.
  "menu/foldera": [
    {
      uri: "http://www.cnn.com",
      title: "CNN",
      changes: { location: "menu/folderd" },
    },
  ],

  // we rename this child (p1) after deleting the folder (p2), and expect the child
  // to be moved into great grandparent (menu)
  "menu/folderb": [
    {
      uri: "http://www.apple.com",
      title: "Apple",
      tags: [],
      changes: { title: "Mac" },
    },
  ],

  // we move this child (p1) after deleting the child (p2) and expect it to survive
  "menu/folderc": [
    {
      uri: "http://www.yahoo.com",
      title: "Yahoo",
      changes: { location: "menu/folderd" },
    },
  ],

  "menu/folderd": [],
};

// a list of bookmarks to delete during a 'delete' action
bookmarksToDelete = {
  menu: [{ folder: "foldera" }, { folder: "folderb" }],
  "menu/folderc": [{ uri: "http://www.yahoo.com", title: "Yahoo" }],
};

// expected bookmark state after conflict resolution
var bookmarksExpected = {
  menu: [
    { folder: "folderc" },
    { folder: "folderd" },
    { uri: "http://www.apple.com", title: "Mac" },
  ],

  "menu/folderc": [],

  "menu/folderd": [
    { uri: "http://www.cnn.com", title: "CNN" },
    { uri: "http://www.yahoo.com", title: "Yahoo" },
  ],
};

// Add bookmarks to profile1 and sync.
Phase("phase1", [
  [Bookmarks.add, bookmarksInitial],
  [Bookmarks.verify, bookmarksInitial],
  [Sync],
  [Bookmarks.verify, bookmarksInitial],
]);

// Sync to profile2 and verify that the bookmarks are present. Delete
// bookmarks/folders, verify that it's not present, and sync
Phase("phase2", [
  [Sync],
  [Bookmarks.verify, bookmarksInitial],
  [Bookmarks.delete, bookmarksToDelete],
  [Bookmarks.verifyNot, bookmarksToDelete],
  [Sync],
]);

// Using profile1, modify the bookmarks, and sync *after* the modification,
// and then sync again to propagate the reconciliation changes.
Phase("phase3", [
  [Bookmarks.verify, bookmarksInitial],
  [Bookmarks.modify, bookmarkMods],
  [Sync],
  [Bookmarks.verify, bookmarksExpected],
  [Bookmarks.verifyNot, bookmarksToDelete],
]);

// Back in profile2, do a sync and verify that we're in the expected state
Phase("phase4", [
  [Sync],
  [Bookmarks.verify, bookmarksExpected],
  [Bookmarks.verifyNot, bookmarksToDelete],
]);
