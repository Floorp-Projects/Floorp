/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Services.jsm");

var Cc = Components.classes;
var Ci = Components.interfaces;

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "10.0");

var prefService = Services.prefs;
var appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                 getService(Ci.nsIAppStartup);

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
  do_check_false(gAppInfo.inSafeMode);

  // first run with startup crash tracking - existing profile lock
  let replacedLockTime = Date.now();
  resetTestEnv(replacedLockTime);
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
  do_check_false(prefService.prefHasUserValue(pref_last_success));
  do_check_eq(replacedLockTime, gAppInfo.replacedLockTime);
  try {
    do_check_false(appStartup.trackStartupCrashBegin());
    do_throw("Should have thrown since last_success is not set");
  } catch (x) { }

  do_check_false(prefService.prefHasUserValue(pref_last_success));
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
  do_check_false(appStartup.automaticSafeModeNecessary);

  // first run with startup crash tracking - no existing profile lock
  replacedLockTime = 0;
  resetTestEnv(replacedLockTime);
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
  do_check_false(prefService.prefHasUserValue(pref_last_success));
  do_check_eq(replacedLockTime, gAppInfo.replacedLockTime);
  try {
    do_check_false(appStartup.trackStartupCrashBegin());
    do_throw("Should have thrown since last_success is not set");
  } catch (x) { }

  do_check_false(prefService.prefHasUserValue(pref_last_success));
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
  do_check_false(appStartup.automaticSafeModeNecessary);

  // normal startup - last startup was success
  replacedLockTime = Date.now();
  resetTestEnv(replacedLockTime);
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  do_check_eq(ms_to_s(replacedLockTime), prefService.getIntPref(pref_last_success));
  do_check_false(appStartup.trackStartupCrashBegin());
  do_check_eq(ms_to_s(replacedLockTime), prefService.getIntPref(pref_last_success));
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
  do_check_false(appStartup.automaticSafeModeNecessary);

  // normal startup with 1 recent crash
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_recent_crashes, 1);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  do_check_false(appStartup.trackStartupCrashBegin());
  do_check_eq(ms_to_s(replacedLockTime), prefService.getIntPref(pref_last_success));
  do_check_eq(1, prefService.getIntPref(pref_recent_crashes));
  do_check_false(appStartup.automaticSafeModeNecessary);

  // normal startup with max_resumed_crashes crash
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_recent_crashes, max_resumed);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  do_check_false(appStartup.trackStartupCrashBegin());
  do_check_eq(ms_to_s(replacedLockTime), prefService.getIntPref(pref_last_success));
  do_check_eq(max_resumed, prefService.getIntPref(pref_recent_crashes));
  do_check_false(appStartup.automaticSafeModeNecessary);

  // normal startup with too many recent crashes
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_recent_crashes, max_resumed + 1);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  do_check_true(appStartup.trackStartupCrashBegin());
  // should remain the same since the last startup was not a crash
  do_check_eq(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  do_check_true(appStartup.automaticSafeModeNecessary);

  // normal startup with too many recent crashes and startup crash tracking disabled
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_max_resumed_crashes, -1);
  prefService.setIntPref(pref_recent_crashes, max_resumed + 1);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  do_check_false(appStartup.trackStartupCrashBegin());
  // should remain the same since the last startup was not a crash
  do_check_eq(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  // returns false when disabled
  do_check_false(appStartup.automaticSafeModeNecessary);
  do_check_eq(-1, prefService.getIntPref(pref_max_resumed_crashes));

  // normal startup after 1 non-recent crash (1 year ago), no other recent
  replacedLockTime = Date.now() - 365 * 24 * 60 * 60 * 1000;
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 365 * 24 * 60 * 60);
  do_check_false(appStartup.trackStartupCrashBegin());
  // recent crash count pref should be unset since the last crash was not recent
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
  do_check_false(appStartup.automaticSafeModeNecessary);

  // normal startup after 1 crash (1 minute ago), no other recent
  replacedLockTime = Date.now() - 60 * 1000;
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 60 * 60); // last success - 1 hour ago
  do_check_false(appStartup.trackStartupCrashBegin());
  // recent crash count pref should be created with value 1
  do_check_eq(1, prefService.getIntPref(pref_recent_crashes));
  do_check_false(appStartup.automaticSafeModeNecessary);

  // normal startup after another crash (1 minute ago), 1 already
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 60 * 60); // last success - 1 hour ago
  replacedLockTime = Date.now() - 60 * 1000;
  gAppInfo.replacedLockTime = replacedLockTime;
  do_check_false(appStartup.trackStartupCrashBegin());
  // recent crash count pref should be incremented by 1
  do_check_eq(2, prefService.getIntPref(pref_recent_crashes));
  do_check_false(appStartup.automaticSafeModeNecessary);

  // normal startup after another crash (1 minute ago), 2 already
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 60 * 60); // last success - 1 hour ago
  do_check_true(appStartup.trackStartupCrashBegin());
  // recent crash count pref should be incremented by 1
  do_check_eq(3, prefService.getIntPref(pref_recent_crashes));
  do_check_true(appStartup.automaticSafeModeNecessary);

  // normal startup after 1 non-recent crash (1 year ago), 3 crashes already
  replacedLockTime = Date.now() - 365 * 24 * 60 * 60 * 1000;
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime) - 60 * 60); // last success - 1 hour ago
  do_check_false(appStartup.trackStartupCrashBegin());
  // recent crash count should be unset since the last crash was not recent
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
  do_check_false(appStartup.automaticSafeModeNecessary);
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
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
  do_check_false(prefService.prefHasUserValue(pref_last_success));

  // successful startup - should set last_success
  replacedLockTime = Date.now() - 10 * 1000; // 10s ago
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  appStartup.trackStartupCrashBegin(); // required to be called before end
  appStartup.trackStartupCrashEnd();
  // ensure last_success was set since we have declared a succesful startup
  // main timestamp doesn't get set in XPCShell so approximate with now
  do_check_true(prefService.getIntPref(pref_last_success) <= now_seconds());
  do_check_true(prefService.getIntPref(pref_last_success) >= now_seconds() - 4 * 60 * 60);
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));

  // successful startup with 1 recent crash
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  prefService.setIntPref(pref_recent_crashes, 1);
  appStartup.trackStartupCrashBegin(); // required to be called before end
  appStartup.trackStartupCrashEnd();
  // ensure recent_crashes was cleared since we have declared a succesful startup
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
}

function test_trackStartupCrashBegin_safeMode() {
  gAppInfo.inSafeMode = true;
  resetTestEnv(0);
  let max_resumed = prefService.getIntPref(pref_max_resumed_crashes);

  // check manual safe mode doesn't change prefs without crash
  let replacedLockTime = Date.now() - 10 * 1000; // 10s ago
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));

  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
  do_check_true(prefService.prefHasUserValue(pref_last_success));
  do_check_false(appStartup.automaticSafeModeNecessary);
  do_check_false(appStartup.trackStartupCrashBegin());
  do_check_false(prefService.prefHasUserValue(pref_recent_crashes));
  do_check_true(prefService.prefHasUserValue(pref_last_success));
  do_check_false(appStartup.automaticSafeModeNecessary);

  // check forced safe mode doesn't change prefs without crash
  replacedLockTime = Date.now() - 10 * 1000; // 10s ago
  resetTestEnv(replacedLockTime);
  prefService.setIntPref(pref_last_success, ms_to_s(replacedLockTime));
  prefService.setIntPref(pref_recent_crashes, max_resumed + 1);

  do_check_eq(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  do_check_true(prefService.prefHasUserValue(pref_last_success));
  do_check_false(appStartup.automaticSafeModeNecessary);
  do_check_true(appStartup.trackStartupCrashBegin());
  do_check_eq(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  do_check_true(prefService.prefHasUserValue(pref_last_success));
  do_check_true(appStartup.automaticSafeModeNecessary);

  // check forced safe mode after old crash
  replacedLockTime = Date.now() - 365 * 24 * 60 * 60 * 1000;
  resetTestEnv(replacedLockTime);
  // one year ago
  let last_success = ms_to_s(replacedLockTime) - 365 * 24 * 60 * 60;
  prefService.setIntPref(pref_last_success, last_success);
  prefService.setIntPref(pref_recent_crashes, max_resumed + 1);
  do_check_eq(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  do_check_true(prefService.prefHasUserValue(pref_last_success));
  do_check_true(appStartup.automaticSafeModeNecessary);
  do_check_true(appStartup.trackStartupCrashBegin());
  do_check_eq(max_resumed + 1, prefService.getIntPref(pref_recent_crashes));
  do_check_eq(last_success, prefService.getIntPref(pref_last_success));
  do_check_true(appStartup.automaticSafeModeNecessary);
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
  do_check_eq(1, prefService.getIntPref(pref_recent_crashes));

  // recent_crashes should be set to max_resumed in forced safe mode to allow the user
  // to try and start in regular mode after making changes.
  prefService.setIntPref(pref_recent_crashes, max_resumed + 1);
  appStartup.trackStartupCrashBegin(); // required to be called before end
  appStartup.trackStartupCrashEnd();
  do_check_eq(max_resumed, prefService.getIntPref(pref_recent_crashes));
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
  do_check_eq(max_resumed + 2, prefService.getIntPref(pref_recent_crashes));
  do_check_false(appStartup.automaticSafeModeNecessary);
}
