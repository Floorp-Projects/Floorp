/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in strict JSON format, as it will get parsed by the Python
 * testrunner (no single quotes, extra comma's, etc).
 */
EnableEngines(["prefs"]);

var phases = { "phase1": "profile1",
               "phase2": "profile2",
               "phase3": "profile1"};

var prefs1 = [
  { name: "browser.startup.homepage",
    value: "http://www.getfirefox.com"
  },
  { name: "browser.urlbar.maxRichResults",
    value: 20
  },
  { name: "security.OCSP.require",
    value: true
  }
];

var prefs2 = [
  { name: "browser.startup.homepage",
    value: "http://www.mozilla.com"
  },
  { name: "browser.urlbar.maxRichResults",
    value: 18
  },
  { name: "security.OCSP.require",
    value: false
  }
];

Phase('phase1', [
  [Prefs.modify, prefs1],
  [Prefs.verify, prefs1],
  [Sync],
]);

Phase('phase2', [
  [Sync],
  [Prefs.verify, prefs1],
  [Prefs.modify, prefs2],
  [Prefs.verify, prefs2],
  [Sync]
]);

Phase('phase3', [
  [Sync],
  [Prefs.verify, prefs2]
]);

