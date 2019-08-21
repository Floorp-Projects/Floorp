/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

EnableEngines(["tabs"]);

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in JSON format as it will get parsed by the Python
 * testrunner. It is parsed by the YAML package, so it relatively flexible.
 */
var phases = { phase1: "profile1", phase2: "profile2" };

/*
 * Tabs data
 */

var tabs1 = [
  { uri: "about:config", profile: "profile1" },
  { uri: "about:credits", profile: "profile1" },
  {
    uri:
      "data:text/html,<html><head><title>Apple</title></head><body>Apple</body></html>",
    title: "Apple",
    profile: "profile1",
  },
];

var tabs_absent = [
  { uri: "about:config", profile: "profile1" },
  { uri: "about:credits", profile: "profile1" },
];

/*
 * Test phases
 */

Phase("phase1", [[Tabs.add, tabs1], [Sync]]);

Phase("phase2", [[Sync], [Tabs.verifyNot, tabs_absent]]);
