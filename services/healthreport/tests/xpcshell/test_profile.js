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
  includeProfileReset: false,

  getProfileDays: function getProfileDays() {
    let result = {profileCreation: 1234};
    if (this.includeProfileReset) {
      result.profileReset = 5678;
    }
    return Promise.resolve(result);
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
  let acc = new ProfileTimesAccessor();
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

  let created = yield acc.computeAndPersistCreated(existing, "test2.json")
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

add_task(function* test_time_reset() {
  let lower = profile_creation_lower;
  let acc = getAccessor();
  let testTime = 100000;
  yield acc.recordProfileReset(testTime);
  let reset = yield acc.reset;
  Assert.equal(reset, testTime);
});

add_test(function test_constructor() {
  let provider = new ProfileMetadataProvider("named");
  run_next_test();
});

add_test(function test_profile_files() {
  let provider = new ProfileMetadataProvider();

  function onSuccess(answer) {
    let now = Date.now() / MILLISECONDS_PER_DAY;
    print("Got " + answer.profileCreation + ", versus now = " + now);
    Assert.ok(answer.profileCreation < now);
    run_next_test();
  }

  function onFailure(ex) {
    do_throw("Directory iteration failed: " + ex);
  }

  provider.getProfileDays().then(onSuccess, onFailure);
});

// A generic test helper. We use this with both real
// and mock providers in these tests.
function test_collect_constant(provider, expectReset) {
  return Task.spawn(function* () {
    yield provider.collectConstantData();

    let m = provider.getMeasurement("age", 2);
    Assert.notEqual(m, null);
    let values = yield m.getValues();
    Assert.ok(values.singular.has("profileCreation"));
    let createValue = values.singular.get("profileCreation")[1];
    let resetValue;
    if (expectReset) {
      Assert.equal(values.singular.size, 2);
      Assert.ok(values.singular.has("profileReset"));
      resetValue = values.singular.get("profileReset")[1];
    } else {
      Assert.equal(values.singular.size, 1);
      Assert.ok(!values.singular.has("profileReset"));
    }
    return [createValue, resetValue];
  });
}

add_task(function* test_collect_constant_mock_no_reset() {
  let storage = yield Metrics.Storage("collect_constant_mock");
  let provider = new MockProfileMetadataProvider();
  yield provider.init(storage);

  let v = yield test_collect_constant(provider, false);
  Assert.equal(v.length, 2);
  Assert.equal(v[0], 1234);
  Assert.equal(v[1], undefined);

  yield storage.close();
});

add_task(function* test_collect_constant_mock_with_reset() {
  let storage = yield Metrics.Storage("collect_constant_mock");
  let provider = new MockProfileMetadataProvider();
  provider.includeProfileReset = true;
  yield provider.init(storage);

  let v = yield test_collect_constant(provider, true);
  Assert.equal(v.length, 2);
  Assert.equal(v[0], 1234);
  Assert.equal(v[1], 5678);

  yield storage.close();
});

add_task(function* test_collect_constant_real_no_reset() {
  let provider = new ProfileMetadataProvider();
  let storage = yield Metrics.Storage("collect_constant_real");
  yield provider.init(storage);

  let vals = yield test_collect_constant(provider, false);
  let created = vals[0];
  let reset = vals[1];
  Assert.equal(reset, undefined);

  let ms = created * MILLISECONDS_PER_DAY;
  let lower = profile_creation_lower;
  let upper = Date.now() + 1000;
  print("Day:   " + created);
  print("msec:  " + ms);
  print("Lower: " + lower);
  print("Upper: " + upper);
  Assert.ok(lower <= ms);
  Assert.ok(upper >= ms);

  yield storage.close();
});

add_task(function* test_collect_constant_real_with_reset() {
  let now = Date.now();
  let acc = getAccessor();
  yield acc.writeTimes({created: now-MILLISECONDS_PER_DAY, // yesterday
                        reset: Date.now()}); // today

  let provider = new ProfileMetadataProvider();
  let storage = yield Metrics.Storage("collect_constant_real");
  yield provider.init(storage);

  let [created, reset] = yield test_collect_constant(provider, true);
  // we've already tested truncate() works as expected, so here just check
  // we got values.
  Assert.ok(created);
  Assert.ok(reset);
  Assert.ok(created <= reset);

  yield storage.close();
});

