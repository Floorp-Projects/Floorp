/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var test_generator = do_run_test();

function run_test() {
  do_test_pending();
  do_run_generator(test_generator);
}

function continue_test() {
  do_run_generator(test_generator);
}

function repeat_test() {
  // The test is probably going to fail because setting a batch of cookies took
  // a significant fraction of 'gPurgeAge'. Compensate by rerunning the
  // test with a larger purge age.
  Assert.ok(gPurgeAge < 64);
  gPurgeAge *= 2;
  gShortExpiry *= 2;

  executeSoon(function() {
    test_generator.return();
    test_generator = do_run_test();
    do_run_generator(test_generator);
  });
}

// Purge threshold, in seconds.
var gPurgeAge = 1;

// Short expiry age, in seconds.
var gShortExpiry = 2;

// Required delay to ensure a purge occurs, in milliseconds. This must be at
// least gPurgeAge + 10%, and includes a little fuzz to account for timer
// resolution and possible differences between PR_Now() and Date.now().
function get_purge_delay() {
  return gPurgeAge * 1100 + 100;
}

// Required delay to ensure a cookie set with an expiry time 'gShortExpiry' into
// the future will have expired.
function get_expiry_delay() {
  return gShortExpiry * 1000 + 100;
}

function* do_run_test() {
  // Set up a profile.
  do_get_profile();

  // twiddle prefs to convenient values for this test
  Services.prefs.setIntPref("network.cookie.purgeAge", gPurgeAge);
  Services.prefs.setIntPref("network.cookie.maxNumber", 100);

  let expiry = Date.now() / 1000 + 1000;

  // eviction is performed based on two limits: when the total number of cookies
  // exceeds maxNumber + 10% (110), and when cookies are older than purgeAge
  // (1 second). purging is done when both conditions are satisfied, and only
  // those cookies are purged.

  // we test the following cases of eviction:
  // 1) excess and age are satisfied, but only some of the excess are old enough
  // to be purged.
  Services.cookiemgr.removeAll();
  if (!set_cookies(0, 5, expiry)) {
    repeat_test();
    return;
  }
  // Sleep a while, to make sure the first batch of cookies is older than
  // the second (timer resolution varies on different platforms).
  do_timeout(get_purge_delay(), continue_test);
  yield;
  if (!set_cookies(5, 111, expiry)) {
    repeat_test();
    return;
  }

  // Fake a profile change, to ensure eviction affects the database correctly.
  do_close_profile(test_generator);
  yield;
  do_load_profile();
  Assert.ok(check_remaining_cookies(111, 5, 106));

  // 2) excess and age are satisfied, and all of the excess are old enough
  // to be purged.
  Services.cookiemgr.removeAll();
  if (!set_cookies(0, 10, expiry)) {
    repeat_test();
    return;
  }
  do_timeout(get_purge_delay(), continue_test);
  yield;
  if (!set_cookies(10, 111, expiry)) {
    repeat_test();
    return;
  }

  do_close_profile(test_generator);
  yield;
  do_load_profile();
  Assert.ok(check_remaining_cookies(111, 10, 101));

  // 3) excess and age are satisfied, and more than the excess are old enough
  // to be purged.
  Services.cookiemgr.removeAll();
  if (!set_cookies(0, 50, expiry)) {
    repeat_test();
    return;
  }
  do_timeout(get_purge_delay(), continue_test);
  yield;
  if (!set_cookies(50, 111, expiry)) {
    repeat_test();
    return;
  }

  do_close_profile(test_generator);
  yield;
  do_load_profile();
  Assert.ok(check_remaining_cookies(111, 50, 101));

  // 4) excess but not age are satisfied.
  Services.cookiemgr.removeAll();
  if (!set_cookies(0, 120, expiry)) {
    repeat_test();
    return;
  }

  do_close_profile(test_generator);
  yield;
  do_load_profile();
  Assert.ok(check_remaining_cookies(120, 0, 120));

  // 5) age but not excess are satisfied.
  Services.cookiemgr.removeAll();
  if (!set_cookies(0, 20, expiry)) {
    repeat_test();
    return;
  }
  do_timeout(get_purge_delay(), continue_test);
  yield;
  if (!set_cookies(20, 110, expiry)) {
    repeat_test();
    return;
  }

  do_close_profile(test_generator);
  yield;
  do_load_profile();
  Assert.ok(check_remaining_cookies(110, 20, 110));

  // 6) Excess and age are satisfied, but the cookie limit can be satisfied by
  // purging expired cookies.
  Services.cookiemgr.removeAll();
  let shortExpiry = Math.floor(Date.now() / 1000) + gShortExpiry;
  if (!set_cookies(0, 20, shortExpiry)) {
    repeat_test();
    return;
  }
  do_timeout(get_expiry_delay(), continue_test);
  yield;
  if (!set_cookies(20, 110, expiry)) {
    repeat_test();
    return;
  }
  do_timeout(get_purge_delay(), continue_test);
  yield;
  if (!set_cookies(110, 111, expiry)) {
    repeat_test();
    return;
  }

  do_close_profile(test_generator);
  yield;
  do_load_profile();
  Assert.ok(check_remaining_cookies(111, 20, 91));

  do_finish_generator_test(test_generator);
}

// Set 'end - begin' total cookies, with consecutively increasing hosts numbered
// 'begin' to 'end'.
function set_cookies(begin, end, expiry) {
  Assert.ok(begin != end);

  let beginTime;
  for (let i = begin; i < end; ++i) {
    let host = "eviction." + i + ".tests";
    Services.cookiemgr.add(
      host,
      "",
      "test",
      "eviction",
      false,
      false,
      false,
      expiry,
      {},
      Ci.nsICookie.SAMESITE_NONE,
      Ci.nsICookie.SCHEME_HTTPS
    );

    if (i == begin) {
      beginTime = get_creationTime(i);
    }
  }

  let endTime = get_creationTime(end - 1);
  Assert.ok(begin == end - 1 || endTime > beginTime);
  if (endTime - beginTime > gPurgeAge * 1000000) {
    // Setting cookies took an amount of time very close to the purge threshold.
    // Retry the test with a larger threshold.
    return false;
  }

  return true;
}

function get_creationTime(i) {
  let host = "eviction." + i + ".tests";
  let cookies = Services.cookiemgr.getCookiesFromHost(host, {});
  Assert.ok(cookies.length);
  let cookie = cookies[0];
  return cookie.creationTime;
}

// Test that 'aNumberToExpect' cookies remain after purging is complete, and
// that the cookies that remain consist of the set expected given the number of
// of older and newer cookies -- eviction should occur by order of lastAccessed
// time, if both the limit on total cookies (maxNumber + 10%) and the purge age
// + 10% are exceeded.
function check_remaining_cookies(aNumberTotal, aNumberOld, aNumberToExpect) {
  let i = 0;
  for (let cookie of Services.cookiemgr.cookies) {
    ++i;

    if (aNumberTotal != aNumberToExpect) {
      // make sure the cookie is one of the batch we expect was purged.
      var hostNumber = Number(cookie.rawHost.split(".")[1]);
      if (hostNumber < aNumberOld - aNumberToExpect) {
        break;
      }
    }
  }

  return i == aNumberToExpect;
}
