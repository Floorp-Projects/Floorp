/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// bug 558077

EnableEngines(["bookmarks"]);

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in JSON format as it will get parsed by the Python
 * testrunner. It is parsed by the YAML package, so it relatively flexible.
 */
var phases = { phase1: "profile1", phase2: "profile2", phase3: "profile1" };

var bookmarks_initial_1 = {
  menu: [
    { folder: "aaa", description: "foo" },
    { uri: "http://www.mozilla.com" },
  ],
  "menu/aaa": [
    { uri: "http://www.yahoo.com", title: "testing Yahoo" },
    { uri: "http://www.google.com", title: "testing Google" },
  ],
};

var bookmarks_initial_2 = {
  menu: [
    { folder: "aaa", description: "bar" },
    { uri: "http://www.mozilla.com" },
  ],
  "menu/aaa": [
    {
      uri: "http://bugzilla.mozilla.org/show_bug.cgi?id=%s",
      title: "Bugzilla",
    },
    { uri: "http://www.apple.com", tags: ["apple"] },
  ],
};

Phase("phase1", [[Bookmarks.add, bookmarks_initial_1], [Sync]]);

Phase("phase2", [
  [Sync],
  [Bookmarks.verify, bookmarks_initial_1],
  [Bookmarks.add, bookmarks_initial_2],
  [Sync],
]);

Phase("phase3", [
  [Sync],
  // XXX [Bookmarks.verify, bookmarks_initial_1],
  [Bookmarks.verify, bookmarks_initial_2],
]);
