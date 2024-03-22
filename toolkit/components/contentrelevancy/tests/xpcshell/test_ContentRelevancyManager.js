/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ContentRelevancyManager:
    "resource://gre/modules/ContentRelevancyManager.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

const PREF_CONTENT_RELEVANCY_ENABLED = "toolkit.contentRelevancy.enabled";
const PREF_TIMER_INTERVAL = "toolkit.contentRelevancy.timerInterval";

// These consts are copied from the update timer manager test. See
// `initUpdateTimerManager()`.
const PREF_APP_UPDATE_TIMERMINIMUMDELAY = "app.update.timerMinimumDelay";
const PREF_APP_UPDATE_TIMERFIRSTINTERVAL = "app.update.timerFirstInterval";
const MAIN_TIMER_INTERVAL = 1000; // milliseconds
const CATEGORY_UPDATE_TIMER = "update-timer";

let gSandbox;

add_setup(() => {
  gSandbox = sinon.createSandbox();
  initUpdateTimerManager();
  Services.prefs.setBoolPref(PREF_CONTENT_RELEVANCY_ENABLED, true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PREF_CONTENT_RELEVANCY_ENABLED);
    gSandbox.restore();
  });
});

add_task(async function test_init() {
  ContentRelevancyManager.init();

  Assert.ok(ContentRelevancyManager.initialized, "Init should succeed");
});

add_task(async function test_uninit() {
  ContentRelevancyManager.init();
  ContentRelevancyManager.uninit();

  Assert.ok(!ContentRelevancyManager.initialized, "Uninit should succeed");
});

add_task(async function test_timer() {
  // Set the timer interval to 0 will trigger the timer right away.
  Services.prefs.setIntPref(PREF_TIMER_INTERVAL, 0);
  gSandbox.spy(ContentRelevancyManager, "notify");

  ContentRelevancyManager.init();

  await TestUtils.waitForCondition(
    () => ContentRelevancyManager.notify.calledOnce,
    "The timer callback should be called"
  );

  Services.prefs.clearUserPref(PREF_TIMER_INTERVAL);
  gSandbox.restore();
});

add_task(async function test_feature_toggling() {
  Services.prefs.setBoolPref(PREF_CONTENT_RELEVANCY_ENABLED, false);
  // Set the timer interval to 0 will trigger the timer right away.
  Services.prefs.setIntPref(PREF_TIMER_INTERVAL, 0);
  gSandbox.spy(ContentRelevancyManager, "notify");

  ContentRelevancyManager.init();

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1100));
  Assert.ok(
    ContentRelevancyManager.notify.notCalled,
    "Timer should not be registered if disabled"
  );

  // Toggle the pref again should re-enable the feature.
  Services.prefs.setBoolPref(PREF_CONTENT_RELEVANCY_ENABLED, true);
  await TestUtils.waitForTick();

  await TestUtils.waitForCondition(
    () => ContentRelevancyManager.notify.calledOnce,
    "The timer callback should be called"
  );

  Services.prefs.clearUserPref(PREF_CONTENT_RELEVANCY_ENABLED);
  Services.prefs.clearUserPref(PREF_TIMER_INTERVAL);
  gSandbox.restore();
});

add_task(async function test_call_disable_twice() {
  ContentRelevancyManager.init();

  Services.prefs.setBoolPref(PREF_CONTENT_RELEVANCY_ENABLED, false);
  await TestUtils.waitForTick();

  Services.prefs.setBoolPref(PREF_CONTENT_RELEVANCY_ENABLED, false);
  await TestUtils.waitForTick();

  Assert.ok(true, "`#disable` should be safe to call multiple times");
});

/**
 * Sets up the update timer manager for testing: makes it fire more often,
 * removes all existing timers, and initializes it for testing. The body of this
 * function is copied from:
 * https://searchfox.org/mozilla-central/source/toolkit/components/timermanager/tests/unit/consumerNotifications.js
 */
function initUpdateTimerManager() {
  // Set the timer to fire every second
  Services.prefs.setIntPref(
    PREF_APP_UPDATE_TIMERMINIMUMDELAY,
    MAIN_TIMER_INTERVAL / 1000
  );
  Services.prefs.setIntPref(
    PREF_APP_UPDATE_TIMERFIRSTINTERVAL,
    MAIN_TIMER_INTERVAL
  );

  // Remove existing update timers to prevent them from being notified
  for (let { data: entry } of Services.catMan.enumerateCategory(
    CATEGORY_UPDATE_TIMER
  )) {
    Services.catMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, entry, false);
  }

  Cc["@mozilla.org/updates/timer-manager;1"]
    .getService(Ci.nsIUpdateTimerManager)
    .QueryInterface(Ci.nsIObserver)
    .observe(null, "utm-test-init", "");
}
