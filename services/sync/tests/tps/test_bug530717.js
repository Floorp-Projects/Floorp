/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

EnableEngines(["prefs"]);

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in JSON format as it will get parsed by the Python
 * testrunner. It is parsed by the YAML package, so it relatively flexible.
 */
var phases = { phase1: "profile1", phase2: "profile2", phase3: "profile1" };

/*
 * Preference lists
 */

var prefs1 = [
  { name: "browser.startup.homepage", value: "http://www.getfirefox.com" },
  { name: "browser.urlbar.maxRichResults", value: 20 },
  { name: "privacy.clearOnShutdown.siteSettings", value: true },
];

var prefs2 = [
  { name: "browser.startup.homepage", value: "http://www.mozilla.com" },
  { name: "browser.urlbar.maxRichResults", value: 18 },
  { name: "privacy.clearOnShutdown.siteSettings", value: false },
];

/*
 * Test phases
 */

// Add prefs to profile1 and sync.
Phase("phase1", [[Prefs.modify, prefs1], [Prefs.verify, prefs1], [Sync]]);

// Sync profile2 and verify same prefs are present.
Phase("phase2", [[Sync], [Prefs.verify, prefs1]]);

// Using profile1, change some prefs, then do another sync with wipe-client.
// Verify that the cloud's prefs are restored, and the recent local changes
// discarded.
Phase("phase3", [
  [Prefs.modify, prefs2],
  [Prefs.verify, prefs2],
  [Sync, SYNC_WIPE_CLIENT],
  [Prefs.verify, prefs1],
]);
