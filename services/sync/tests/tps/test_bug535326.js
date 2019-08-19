/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

EnableEngines(["tabs"]);

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in JSON format as it will get parsed by the Python
 * testrunner. It is parsed by the YAML package, so it relatively flexible.
 */
var phases = { phase1: "profile1", phase2: "profile2" };

var tabs1 = [
  {
    uri:
      "data:text/html,<html><head><title>Howdy</title></head><body>Howdy</body></html>",
    title: "Howdy",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>America</title></head><body>America</body></html>",
    title: "America",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Apple</title></head><body>Apple</body></html>",
    title: "Apple",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>This</title></head><body>This</body></html>",
    title: "This",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Bug</title></head><body>Bug</body></html>",
    title: "Bug",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>IRC</title></head><body>IRC</body></html>",
    title: "IRC",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Tinderbox</title></head><body>Tinderbox</body></html>",
    title: "Tinderbox",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Fox</title></head><body>Fox</body></html>",
    title: "Fox",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Hello</title></head><body>Hello</body></html>",
    title: "Hello",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Eagle</title></head><body>Eagle</body></html>",
    title: "Eagle",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Train</title></head><body>Train</body></html>",
    title: "Train",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Macbook</title></head><body>Macbook</body></html>",
    title: "Macbook",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Clock</title></head><body>Clock</body></html>",
    title: "Clock",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Google</title></head><body>Google</body></html>",
    title: "Google",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Human</title></head><body>Human</body></html>",
    title: "Human",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Jetpack</title></head><body>Jetpack</body></html>",
    title: "Jetpack",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Selenium</title></head><body>Selenium</body></html>",
    title: "Selenium",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Mozilla</title></head><body>Mozilla</body></html>",
    title: "Mozilla",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Firefox</title></head><body>Firefox</body></html>",
    title: "Firefox",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Weave</title></head><body>Weave</body></html>",
    title: "Weave",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Android</title></head><body>Android</body></html>",
    title: "Android",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Bye</title></head><body>Bye</body></html>",
    title: "Bye",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Hi</title></head><body>Hi</body></html>",
    title: "Hi",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Final</title></head><body>Final</body></html>",
    title: "Final",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Fennec</title></head><body>Fennec</body></html>",
    title: "Fennec",
    profile: "profile1",
  },
  {
    uri:
      "data:text/html,<html><head><title>Mobile</title></head><body>Mobile</body></html>",
    title: "Mobile",
    profile: "profile1",
  },
];

Phase("phase1", [[Tabs.add, tabs1], [Sync]]);

Phase("phase2", [[Sync], [Tabs.verify, tabs1]]);
