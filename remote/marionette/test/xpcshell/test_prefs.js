/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "env",
  "@mozilla.org/process/environment;1",
  "nsIEnvironment"
);

const { Branch, EnvironmentPrefs, MarionettePrefs } = ChromeUtils.import(
  "chrome://remote/content/marionette/prefs.js"
);

function reset() {
  Services.prefs.setBoolPref("test.bool", false);
  Services.prefs.setStringPref("test.string", "foo");
  Services.prefs.setIntPref("test.int", 777);
}

// Give us something to work with:
reset();

add_test(function test_Branch_get_root() {
  let root = new Branch(null);
  equal(false, root.get("test.bool"));
  equal("foo", root.get("test.string"));
  equal(777, root.get("test.int"));
  Assert.throws(() => root.get("doesnotexist"), /TypeError/);

  run_next_test();
});

add_test(function test_Branch_get_branch() {
  let test = new Branch("test.");
  equal(false, test.get("bool"));
  equal("foo", test.get("string"));
  equal(777, test.get("int"));
  Assert.throws(() => test.get("doesnotexist"), /TypeError/);

  run_next_test();
});

add_test(function test_Branch_set_root() {
  let root = new Branch(null);

  try {
    root.set("test.string", "bar");
    root.set("test.in", 777);
    root.set("test.bool", true);

    equal("bar", Services.prefs.getStringPref("test.string"));
    equal(777, Services.prefs.getIntPref("test.int"));
    equal(true, Services.prefs.getBoolPref("test.bool"));
  } finally {
    reset();
  }

  run_next_test();
});

add_test(function test_Branch_set_branch() {
  let test = new Branch("test.");

  try {
    test.set("string", "bar");
    test.set("int", 888);
    test.set("bool", true);

    equal("bar", Services.prefs.getStringPref("test.string"));
    equal(888, Services.prefs.getIntPref("test.int"));
    equal(true, Services.prefs.getBoolPref("test.bool"));
  } finally {
    reset();
  }

  run_next_test();
});

add_test(function test_EnvironmentPrefs_from() {
  let prefsTable = {
    "test.bool": true,
    "test.int": 888,
    "test.string": "bar",
  };
  env.set("FOO", JSON.stringify(prefsTable));

  try {
    for (let [key, value] of EnvironmentPrefs.from("FOO")) {
      equal(prefsTable[key], value);
    }
  } finally {
    env.set("FOO", null);
  }

  run_next_test();
});

add_test(function test_MarionettePrefs_getters() {
  equal(false, MarionettePrefs.clickToStart);
  equal(2828, MarionettePrefs.port);

  run_next_test();
});

add_test(function test_MarionettePrefs_setters() {
  try {
    MarionettePrefs.port = 777;
    equal(777, MarionettePrefs.port);
  } finally {
    Services.prefs.clearUserPref("marionette.port");
  }

  run_next_test();
});
