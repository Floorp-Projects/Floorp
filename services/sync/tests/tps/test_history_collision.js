/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

EnableEngines(["history"]);

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
 * History lists
 */

// the initial list of history to add to the browser
var history1 = [
  {
    uri: "http://www.google.com/",
    title: "Google",
    visits: [{ type: 1, date: 0 }],
  },
  {
    uri: "http://www.cnn.com/",
    title: "CNN",
    visits: [{ type: 1, date: -1 }, { type: 2, date: -36 }],
  },
  {
    uri: "http://www.mozilla.com/",
    title: "Mozilla",
    visits: [{ type: 1, date: 0 }, { type: 2, date: -36 }],
  },
];

// the history to delete
var history_to_delete = [
  { uri: "http://www.cnn.com/", title: "CNN" },
  { begin: -36, end: -1 },
];

var history_not = [
  {
    uri: "http://www.cnn.com/",
    title: "CNN",
    visits: [{ type: 1, date: -1 }, { type: 2, date: -36 }],
  },
];

var history_after_delete = [
  {
    uri: "http://www.google.com/",
    title: "Google",
    visits: [{ type: 1, date: 0 }],
  },
  {
    uri: "http://www.mozilla.com/",
    title: "Mozilla",
    visits: [{ type: 1, date: 0 }],
  },
];

/*
 * Test phases
 */

Phase("phase1", [[History.add, history1], [Sync]]);

Phase("phase2", [[Sync], [History.add, history1], [Sync, SYNC_WIPE_REMOTE]]);

Phase("phase3", [
  [Sync],
  [History.verify, history1],
  [History.delete, history_to_delete],
  [History.verify, history_after_delete],
  [History.verifyNot, history_not],
  [Sync],
]);

Phase("phase4", [
  [Sync],
  [History.verify, history_after_delete],
  [History.verifyNot, history_not],
]);
