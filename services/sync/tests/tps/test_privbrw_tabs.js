/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in strict JSON format, as it will get parsed by the Python
 * testrunner (no single quotes, extra comma's, etc).
 */

var phases = { "phase1": "profile1",
               "phase2": "profile2",
               "phase3": "profile1",
               "phase4": "profile2" };

/*
 * Tabs data
 */

var tabs1 = [
  { uri: "data:text/html,<html><head><title>Firefox</title></head><body>Firefox</body></html>",
    title: "Firefox",
    profile: "profile1"
  },
  { uri: "data:text/html,<html><head><title>Weave</title></head><body>Weave</body></html>",
    title: "Weave",
    profile: "profile1"
  },
  { uri: "data:text/html,<html><head><title>Apple</title></head><body>Apple</body></html>",
    title: "Apple",
    profile: "profile1"
  },
  { uri: "data:text/html,<html><head><title>IRC</title></head><body>IRC</body></html>",
    title: "IRC",
    profile: "profile1"
  }
];

var tabs2 = [
  { uri: "data:text/html,<html><head><title>Tinderbox</title></head><body>Tinderbox</body></html>",
    title: "Tinderbox",
    profile: "profile2"
  },
  { uri: "data:text/html,<html><head><title>Fox</title></head><body>Fox</body></html>",
    title: "Fox",
    profile: "profile2"
  }
];

var tabs3 = [
  { uri: "data:text/html,<html><head><title>Jetpack</title></head><body>Jetpack</body></html>",
    title: "Jetpack",
    profile: "profile1"
  },
  { uri: "data:text/html,<html><head><title>Selenium</title></head><body>Selenium</body></html>",
    title: "Selenium",
    profile: "profile1"
  }
];


/*
 * Test phases
 */

Phase('phase1', [
  [Tabs.add, tabs1],
  [Sync, SYNC_WIPE_SERVER]
]);

Phase('phase2', [
  [Sync],
  [Tabs.verify, tabs1],
  [Tabs.add, tabs2],
  [Sync]
]);

Phase('phase3', [
  [Sync],
  [SetPrivateBrowsing, true],
  [Tabs.add, tabs3],
  [Sync]
]);

Phase('phase4', [
  [Sync],
  [Tabs.verifyNot, tabs3]
]);

