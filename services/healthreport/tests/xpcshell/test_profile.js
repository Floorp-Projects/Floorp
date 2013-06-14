/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

// Create profile directory before use.
// It can be no older than a day ago….
let profile_creation_lower = Date.now() - MILLISECONDS_PER_DAY;
do_get_profile();

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/services/healthreport/profile.jsm");
Cu.import("resource://gre/modules/Task.jsm");


function MockProfileMetadataProvider(name="MockProfileMetadataProvider") {
  this.name = name;
  ProfileMetadataProvider.call(this);
}
MockProfileMetadataProvider.prototype = {
  __proto__: ProfileMetadataProvider.prototype,

  getProfileCreationDays: function getProfileCreationDays() {
    return Promise.resolve(1234);
  },
};


function run_test() {
  run_next_test();
}

/**
 * Ensure that OS.File works in our environment.
 * This test can go once there are xpcshell tests for OS.File.
 */
add_test(function use_os_file() {
  Cu.import("resource://gre/modules/osfile.jsm")

  // Ensure that we get constants, too.
  do_check_neq(OS.Constants.Path.profileDir, null);

  let iterator = new OS.File.DirectoryIterator(".");
  iterator.forEach(function onEntry(entry) {
    print("Got " + entry.path);
  }).then(function onSuccess() {
    iterator.close();
    print("Done.");
    run_next_test();
  }, function onFail() {
    iterator.close();
    do_throw("Iterating over current directory failed.");
  });
});

function getAccessor() {
  let acc = new ProfileCreationTimeAccessor();
  print("Profile is " + acc.profilePath);
  return acc;
}

add_test(function test_time_accessor_no_file() {
  let acc = getAccessor();

  // There should be no file yet.
  acc.readTimes()
     .then(function onSuccess(json) {
       do_throw("File existed!");
     },
     function onFailure() {
       run_next_test();
     });
});

add_task(function test_time_accessor_named_file() {
  let acc = getAccessor();

  // There should be no file yet.
  yield acc.writeTimes({created: 12345}, "test.json");
  let json = yield acc.readTimes("test.json")
  print("Read: " + JSON.stringify(json));
  do_check_eq(12345, json.created);
});

add_task(function test_time_accessor_creates_file() {
  let lower = profile_creation_lower;

  // Ensure that provided contents are merged, and existing
  // files can be overwritten. These two things occur if we
  // read and then decide that we have to write.
  let acc = getAccessor();
  let existing = {abc: "123", easy: "abc"};
  let expected;

  let created = yield acc.computeAndPersistTimes(existing, "test2.json")
  let upper = Date.now() + 1000;
  print(lower + " < " + created + " <= " + upper);
  do_check_true(lower < created);
  do_check_true(upper >= created);
  expected = created;

  let json = yield acc.readTimes("test2.json")
  print("Read: " + JSON.stringify(json));
  do_check_eq("123", json.abc);
  do_check_eq("abc", json.easy);
  do_check_eq(expected, json.created);
});

add_task(function test_time_accessor_all() {
  let lower = profile_creation_lower;
  let acc = getAccessor();
  let expected;
  let created = yield acc.created
  let upper = Date.now() + 1000;
  do_check_true(lower < created);
  do_check_true(upper >= created);
  expected = created;

  let again = yield acc.created
  do_check_eq(expected, again);
});

add_test(function test_constructor() {
  let provider = new ProfileMetadataProvider("named");
  run_next_test();
});

add_test(function test_profile_files() {
  let provider = new ProfileMetadataProvider();

  function onSuccess(answer) {
    let now = Date.now() / MILLISECONDS_PER_DAY;
    print("Got " + answer + ", versus now = " + now);
    do_check_true(answer < now);
    run_next_test();
  }

  function onFailure(ex) {
    do_throw("Directory iteration failed: " + ex);
  }

  provider.getProfileCreationDays().then(onSuccess, onFailure);
});

// A generic test helper. We use this with both real
// and mock providers in these tests.
function test_collect_constant(provider) {
  return Task.spawn(function () {
    yield provider.collectConstantData();

    let m = provider.getMeasurement("age", 1);
    do_check_neq(m, null);
    let values = yield m.getValues();
    do_check_eq(values.singular.size, 1);
    do_check_true(values.singular.has("profileCreation"));

    throw new Task.Result(values.singular.get("profileCreation")[1]);
  });
}

add_task(function test_collect_constant_mock() {
  let storage = yield Metrics.Storage("collect_constant_mock");
  let provider = new MockProfileMetadataProvider();
  yield provider.init(storage);

  let v = yield test_collect_constant(provider);
  do_check_eq(v, 1234);

  yield storage.close();
});

add_task(function test_collect_constant_real() {
  let provider = new ProfileMetadataProvider();
  let storage = yield Metrics.Storage("collect_constant_real");
  yield provider.init(storage);

  let v = yield test_collect_constant(provider);

  let ms = v * MILLISECONDS_PER_DAY;
  let lower = profile_creation_lower;
  let upper = Date.now() + 1000;
  print("Day:   " + v);
  print("msec:  " + ms);
  print("Lower: " + lower);
  print("Upper: " + upper);
  do_check_true(lower <= ms);
  do_check_true(upper >= ms);

  yield storage.close();
});

