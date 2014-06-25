// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/SharedPreferences.jsm");
Components.utils.import("resource://gre/modules/Promise.jsm");

let _observerId = 0;

function makeObserver() {
  let deferred = Promise.defer();

  let ret = {
    id: _observerId++,
    count: 0,
    promise: deferred.promise,
    observe: function (subject, topic, data) {
      ret.count += 1;
      let msg = { subject: subject,
                  topic: topic,
                  data: data };
      deferred.resolve(msg);
    },
  };

  return ret;
};

add_task(function test_get_set() {
  let branch = SharedPreferences.forAndroid("test");

  branch.setBoolPref("boolKey", true);
  branch.setCharPref("charKey", "string value");
  branch.setIntPref("intKey", 1000);

  do_check_eq(branch.getBoolPref("boolKey"), true);
  do_check_eq(branch.getCharPref("charKey"), "string value");
  do_check_eq(branch.getIntPref("intKey"), 1000);

  branch.setBoolPref("boolKey", false);
  branch.setCharPref("charKey", "different string value");
  branch.setIntPref("intKey", -2000);

  do_check_eq(branch.getBoolPref("boolKey"), false);
  do_check_eq(branch.getCharPref("charKey"), "different string value");
  do_check_eq(branch.getIntPref("intKey"), -2000);

  do_check_eq(typeof(branch.getBoolPref("boolKey")), "boolean");
  do_check_eq(typeof(branch.getCharPref("charKey")), "string");
  do_check_eq(typeof(branch.getIntPref("intKey")), "number");
});

add_task(function test_default() {
  let branch = SharedPreferences.forAndroid();

  branch.setBoolPref("boolKey", true);
  branch.setCharPref("charKey", "string value");
  branch.setIntPref("intKey", 1000);

  do_check_eq(branch.getBoolPref("boolKey"), true);
  do_check_eq(branch.getCharPref("charKey"), "string value");
  do_check_eq(branch.getIntPref("intKey"), 1000);

  branch.setBoolPref("boolKey", false);
  branch.setCharPref("charKey", "different string value");
  branch.setIntPref("intKey", -2000);

  do_check_eq(branch.getBoolPref("boolKey"), false);
  do_check_eq(branch.getCharPref("charKey"), "different string value");
  do_check_eq(branch.getIntPref("intKey"), -2000);

  do_check_eq(typeof(branch.getBoolPref("boolKey")), "boolean");
  do_check_eq(typeof(branch.getCharPref("charKey")), "string");
  do_check_eq(typeof(branch.getIntPref("intKey")), "number");
});

add_task(function test_multiple_branches() {
  let branch1 = SharedPreferences.forAndroid("test1");
  let branch2 = SharedPreferences.forAndroid("test2");

  branch1.setBoolPref("boolKey", true);
  branch2.setBoolPref("boolKey", false);

  do_check_eq(branch1.getBoolPref("boolKey"), true);
  do_check_eq(branch2.getBoolPref("boolKey"), false);

  branch1.setCharPref("charKey", "a value");
  branch2.setCharPref("charKey", "a different value");

  do_check_eq(branch1.getCharPref("charKey"), "a value");
  do_check_eq(branch2.getCharPref("charKey"), "a different value");
});

add_task(function test_add_remove_observer() {
  let branch = SharedPreferences.forAndroid("test");

  branch.setBoolPref("boolKey", false);
  do_check_eq(branch.getBoolPref("boolKey"), false);

  let obs1 = makeObserver();
  branch.addObserver("boolKey", obs1);

  try {
    branch.setBoolPref("boolKey", true);
    do_check_eq(branch.getBoolPref("boolKey"), true);

    let value1 = yield obs1.promise;
    do_check_eq(obs1.count, 1);

    do_check_eq(value1.subject, obs1);
    do_check_eq(value1.topic, "boolKey");
    do_check_eq(typeof(value1.data), "boolean");
    do_check_eq(value1.data, true);
  } finally {
    branch.removeObserver("boolKey", obs1);
  }

  // Make sure the original observer is really gone, or as close as
  // we: install a second observer, wait for it to be notified, and
  // then verify the original observer was *not* notified.  This
  // depends, of course, on the order that observers are notified, but
  // is better than nothing.

  let obs2 = makeObserver();
  branch.addObserver("boolKey", obs2);

  try {
    branch.setBoolPref("boolKey", false);
    do_check_eq(branch.getBoolPref("boolKey"), false);

    let value2 = yield obs2.promise;
    do_check_eq(obs2.count, 1);

    do_check_eq(value2.subject, obs2);
    do_check_eq(value2.topic, "boolKey");
    do_check_eq(typeof(value2.data), "boolean");
    do_check_eq(value2.data, false);

    // Original observer count is preserved.
    do_check_eq(obs1.count, 1);
  } finally {
    branch.removeObserver("boolKey", obs2);
  }
});

add_task(function test_observer_ignores() {
  let branch = SharedPreferences.forAndroid("test");

  branch.setCharPref("charKey", "first value");
  do_check_eq(branch.getCharPref("charKey"), "first value");

  let obs = makeObserver();
  branch.addObserver("charKey", obs);

  try {
    // These should all be ignored.
    branch.setBoolPref("boolKey", true);
    branch.setBoolPref("boolKey", false);
    branch.setIntPref("intKey", -3000);
    branch.setIntPref("intKey", 4000);

    branch.setCharPref("charKey", "a value");
    let value = yield obs.promise;

    // Observer should have been notified exactly once.
    do_check_eq(obs.count, 1);

    do_check_eq(value.subject, obs);
    do_check_eq(value.topic, "charKey");
    do_check_eq(typeof(value.data), "string");
    do_check_eq(value.data, "a value");
  } finally {
    branch.removeObserver("charKey", obs);
  }
});

add_task(function test_observer_ignores_branches() {
  let branch = SharedPreferences.forAndroid("test");

  branch.setCharPref("charKey", "first value");
  do_check_eq(branch.getCharPref("charKey"), "first value");

  let obs = makeObserver();
  branch.addObserver("charKey", obs);

  try {
    // These should all be ignored.
    let branch2 = SharedPreferences.forAndroid("test2");
    branch2.setCharPref("charKey", "a wrong value");
    let branch3 = SharedPreferences.forAndroid("test.2");
    branch3.setCharPref("charKey", "a different wrong value");

    // This should not be ignored.
    branch.setCharPref("charKey", "a value");

    let value = yield obs.promise;

    // Observer should have been notified exactly once.
    do_check_eq(obs.count, 1);

    do_check_eq(value.subject, obs);
    do_check_eq(value.topic, "charKey");
    do_check_eq(typeof(value.data), "string");
    do_check_eq(value.data, "a value");
  } finally {
    branch.removeObserver("charKey", obs);
  }
});

add_task(function test_scopes() {
  let forApp = SharedPreferences.forApp();
  let forProfile = SharedPreferences.forProfile();
  let forProfileName = SharedPreferences.forProfileName("testProfile");
  let forAndroidDefault = SharedPreferences.forAndroid();
  let forAndroidBranch = SharedPreferences.forAndroid("testBranch");

  forApp.setCharPref("charKey", "forApp");
  forProfile.setCharPref("charKey", "forProfile");
  forProfileName.setCharPref("charKey", "forProfileName");
  forAndroidDefault.setCharPref("charKey", "forAndroidDefault");
  forAndroidBranch.setCharPref("charKey", "forAndroidBranch");

  do_check_eq(forApp.getCharPref("charKey"), "forApp");
  do_check_eq(forProfile.getCharPref("charKey"), "forProfile");
  do_check_eq(forProfileName.getCharPref("charKey"), "forProfileName");
  do_check_eq(forAndroidDefault.getCharPref("charKey"), "forAndroidDefault");
  do_check_eq(forAndroidBranch.getCharPref("charKey"), "forAndroidBranch");
});

run_next_test();
