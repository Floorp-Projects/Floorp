/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Services.jsm");

var Cc = Components.classes;
var Ci = Components.interfaces;

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "10.0");

var prefService = Services.prefs;
var appStartup = Services.startup;

const pref_last_success = "toolkit.startup.last_success";
const pref_recent_crashes = "toolkit.startup.recent_crashes";
const pref_max_resumed_crashes = "toolkit.startup.max_resumed_crashes";
const pref_always_use_safe_mode = "toolkit.startup.always_use_safe_mode";

function run_test() {
  prefService.setBoolPref(pref_always_use_safe_mode, true);

  resetTestEnv(0);

  test_trackStartupCrashBegin();
  test_trackStartupCrashEnd();
  test_trackStartupCrashBegin_safeMode();
  test_trackStartupCrashEnd_safeMode();
  test_maxResumed();
  resetTestEnv(0);

  prefService.clearUserPref(pref_always_use_safe_mode);
}

// reset prefs to default
function resetTestEnv(replacedLockTime) {
  try {
    // call begin to reset mStartupCrashTrackingEnded
    appStartup.trackStartupCrashBegin();
  } catch (x) { }
  prefService.setIntPref(pref_max_resumed_crashes, 2);
  prefService.clearUserPref(pref_recent_crashes);
  gAppInfo.replacedLockTime = replacedLockTime;
  prefService.clearUserPref(pref_last_success);
}

function now_seconds() {
  return ms_to_s(Date.now());
}

function ms_to_s(ms) {
  return Math.floor(ms / 1000);
}

function test_trackStartupCrashBegin() {
  let max_resumed = prefService.getIntPref(pref_max_resumed_crashes);
  Assert.ok(!gAppInfo.inSafeMode);

  // first run with startup crash tracking - existing profile lock
  let replacedLockTime = Date.now();
  resetTestEnv(replacedLockTime);
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
  Assert.ok(!prefService.prefHasUserValue(pref_last_success));
  Assert.equal(replacedLockTime, gAppInfo.replacedLockTime);
  try {
    Assert.ok(!appStartup.trackStartupCrashBegin());
    do_throw("Should have thrown since last_success is not set");
  } catch (x) { }

  Assert.ok(!prefService.prefHasUserValue(pref_last_success));
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
  Assert.ok(!appStartup.automaticSafeModeNecessary);

  // first run with startup crash tracking - no existing profile lock
  replacedLockTime = 0;
  resetTestEnv(replacedLockTime);
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
  Assert.ok(!prefService.prefHasUserValue(pref_last_success));
  Assert.equal(replacedLockTime, gAppInfo.replacedLockTime);
  try {
    Assert.ok(!appStartup.trackStartupCrashBegin());
    do_throw("Should have thrown since last_success is not set");
  } catch (x) { }

  Assert.ok(!prefService.prefHasUserValue(pref_last_success));
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
  Assert.ok(!appStartup.automaticSafeModeNecessary);

  // normal startup - last startup was success
  replacedLockTime = Date.now();
  resetTestEnv(replacedLockTime);
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  Assert.equal(ms_to_s(replacedLockTime), prefService.getIntPref(pref_last_success));
  Assert.ok(!appStartup.trackStartupCrashBegin());
  Assert.equal(ms_to_s(replacedLockTime), prefService.getIntPref(pref_last_success));
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
  Assert.ok(!appStartup.automaticSafeModeNecessary);

  // normal startup with 1 recent crash
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_recent_crashes, 1);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  Assert.ok(!appStartup.trackStartupCrashBegin());
  Assert.equal(ms_to_s(replacedLockTime), prefService.getIntPref(pref_last_success));
  Assert.equal(1, prefService.getIntPref(pref_recent_crashes));
  Assert.ok(!appStartup.automaticSafeModeNecessary);

  // normal startup with max_resumed_crashes crash
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_recent_crashes, max_resumed);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  Assert.ok(!appStartup.trackStartupCrashBegin());
  Assert.equal(ms_to_s(replacedLockTime), prefService.getIntPref(pref_last_success));
  Assert.equal(max_resumed, prefService.getIntPref(pref_recent_crashes));
  Assert.ok(!appStartup.automaticSafeModeNecessary);

  // normal startup with too many recent crashes
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_recent_crashes, max_resumed + 1);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  Assert.ok(appStartup.trackStartupCrashBegin());
  // should remain the same since the last startup was not a crash
  Assert.equal(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  Assert.ok(appStartup.automaticSafeModeNecessary);

  // normal startup with too many recent crashes and startup crash tracking disabled
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_max_resumed_crashes, -1);
  prefService.setIntPref(pref_recent_crashes, max_resumed + 1);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  Assert.ok(!appStartup.trackStartupCrashBegin());
  // should remain the same since the last startup was not a crash
  Assert.equal(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  // returns false when disabled
  Assert.ok(!appStartup.automaticSafeModeNecessary);
  Assert.equal(-1, prefService.getIntPref(pref_max_resumed_crashes));

  // normal startup after 1 non-recent crash (1 year ago), no other recent
  replacedLockTime = Date.now() - 365 * 24 * 60 * 60 * 1000;
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 365 * 24 * 60 * 60);
  Assert.ok(!appStartup.trackStartupCrashBegin());
  // recent crash count pref should be unset since the last crash was not recent
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
  Assert.ok(!appStartup.automaticSafeModeNecessary);

  // normal startup after 1 crash (1 minute ago), no other recent
  replacedLockTime = Date.now() - 60 * 1000;
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 60 * 60); // last success - 1 hour ago
  Assert.ok(!appStartup.trackStartupCrashBegin());
  // recent crash count pref should be created with value 1
  Assert.equal(1, prefService.getIntPref(pref_recent_crashes));
  Assert.ok(!appStartup.automaticSafeModeNecessary);

  // normal startup after another crash (1 minute ago), 1 already
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 60 * 60); // last success - 1 hour ago
  replacedLockTime = Date.now() - 60 * 1000;
  gAppInfo.replacedLockTime = replacedLockTime;
  Assert.ok(!appStartup.trackStartupCrashBegin());
  // recent crash count pref should be incremented by 1
  Assert.equal(2, prefService.getIntPref(pref_recent_crashes));
  Assert.ok(!appStartup.automaticSafeModeNecessary);

  // normal startup after another crash (1 minute ago), 2 already
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 60 * 60); // last success - 1 hour ago
  Assert.ok(appStartup.trackStartupCrashBegin());
  // recent crash count pref should be incremented by 1
  Assert.equal(3, prefService.getIntPref(pref_recent_crashes));
  Assert.ok(appStartup.automaticSafeModeNecessary);

  // normal startup after 1 non-recent crash (1 year ago), 3 crashes already
  replacedLockTime = Date.now() - 365 * 24 * 60 * 60 * 1000;
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 60 * 60); // last success - 1 hour ago
  Assert.ok(!appStartup.trackStartupCrashBegin());
  // recent crash count should be unset since the last crash was not recent
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
  Assert.ok(!appStartup.automaticSafeModeNecessary);
}

function test_trackStartupCrashEnd() {
  // successful startup with no last_success (upgrade test)
  let replacedLockTime = Date.now() - 10 * 1000; // 10s ago
  resetTestEnv(replacedLockTime);
  try {
    appStartup.trackStartupCrashBegin(); // required to be called before end
    do_throw("Should have thrown since last_success is not set");
  } catch (x) { }
  appStartup.trackStartupCrashEnd();
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
  Assert.ok(!prefService.prefHasUserValue(pref_last_success));

  // successful startup - should set last_success
  replacedLockTime = Date.now() - 10 * 1000; // 10s ago
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  appStartup.trackStartupCrashBegin(); // required to be called before end
  appStartup.trackStartupCrashEnd();
  // ensure last_success was set since we have declared a succesful startup
  // main timestamp doesn't get set in XPCShell so approximate with now
  Assert.ok(prefService.getIntPref(pref_last_success) <= now_seconds());
  Assert.ok(prefService.getIntPref(pref_last_success) >= now_seconds() - 4 * 60 * 60);
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));

  // successful startup with 1 recent crash
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  prefService.setIntPref(pref_recent_crashes, 1);
  appStartup.trackStartupCrashBegin(); // required to be called before end
  appStartup.trackStartupCrashEnd();
  // ensure recent_crashes was cleared since we have declared a succesful startup
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
}

function test_trackStartupCrashBegin_safeMode() {
  gAppInfo.inSafeMode = true;
  resetTestEnv(0);
  let max_resumed = prefService.getIntPref(pref_max_resumed_crashes);

  // check manual safe mode doesn't change prefs without crash
  let replacedLockTime = Date.now() - 10 * 1000; // 10s ago
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));

  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
  Assert.ok(prefService.prefHasUserValue(pref_last_success));
  Assert.ok(!appStartup.automaticSafeModeNecessary);
  Assert.ok(!appStartup.trackStartupCrashBegin());
  Assert.ok(!prefService.prefHasUserValue(pref_recent_crashes));
  Assert.ok(prefService.prefHasUserValue(pref_last_success));
  Assert.ok(!appStartup.automaticSafeModeNecessary);

  // check forced safe mode doesn't change prefs without crash
  replacedLockTime = Date.now() - 10 * 1000; // 10s ago
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  prefService.setIntPref(pref_recent_crashes, max_resumed + 1);

  Assert.equal(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  Assert.ok(prefService.prefHasUserValue(pref_last_success));
  Assert.ok(!appStartup.automaticSafeModeNecessary);
  Assert.ok(appStartup.trackStartupCrashBegin());
  Assert.equal(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  Assert.ok(prefService.prefHasUserValue(pref_last_success));
  Assert.ok(appStartup.automaticSafeModeNecessary);

  // check forced safe mode after old crash
  replacedLockTime = Date.now() - 365 * 24 * 60 * 60 * 1000;
  resetTestEnv(replacedLockTime);
  // one year ago
  let last_success = ms_to_s(replacedLockTime) - 365 * 24 * 60 * 60;
  prefService.setIntPref(pref_last_success, last_success);
  prefService.setIntPref(pref_recent_crashes, max_resumed + 1);
  Assert.equal(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  Assert.ok(prefService.prefHasUserValue(pref_last_success));
  Assert.ok(appStartup.automaticSafeModeNecessary);
  Assert.ok(appStartup.trackStartupCrashBegin());
  Assert.equal(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  Assert.equal(last_success, prefService.getIntPref(pref_last_success));
  Assert.ok(appStartup.automaticSafeModeNecessary);
}

function test_trackStartupCrashEnd_safeMode() {
  gAppInfo.inSafeMode = true;
  let replacedLockTime = Date.now();
  resetTestEnv(replacedLockTime);
  let max_resumed = prefService.getIntPref(pref_max_resumed_crashes);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 24 * 60 * 60);

  // ensure recent_crashes were not cleared in manual safe mode
  prefService.setIntPref(pref_recent_crashes, 1);
  appStartup.trackStartupCrashBegin(); // required to be called before end
  appStartup.trackStartupCrashEnd();
  Assert.equal(1, prefService.getIntPref(pref_recent_crashes));

  // recent_crashes should be set to max_resumed in forced safe mode to allow the user
  // to try and start in regular mode after making changes.
  prefService.setIntPref(pref_recent_crashes, max_resumed + 1);
  appStartup.trackStartupCrashBegin(); // required to be called before end
  appStartup.trackStartupCrashEnd();
  Assert.equal(max_resumed, prefService.getIntPref(pref_recent_crashes));
}

function test_maxResumed() {
  resetTestEnv(0);
  gAppInfo.inSafeMode = false;
  let max_resumed = prefService.getIntPref(pref_max_resumed_crashes);
  let replacedLockTime = Date.now();
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_max_resumed_crashes, -1);

  prefService.setIntPref(pref_recent_crashes, max_resumed + 1);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 24 * 60 * 60);
  appStartup.trackStartupCrashBegin();
  // should remain the same since the last startup was not a crash
  Assert.equal(max_resumed + 2, prefService.getIntPref(pref_recent_crashes));
  Assert.ok(!appStartup.automaticSafeModeNecessary);
}
