/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gUpdateTimerManager",
  "@mozilla.org/updates/timer-manager;1",
  "nsIUpdateTimerManager"
);

const PREF_APP_UPDATE_LASTUPDATETIME_FMT = "app.update.lastUpdateTime.%ID%";

add_task(async function() {
  const testId = "test_timer_id";
  const testPref = PREF_APP_UPDATE_LASTUPDATETIME_FMT.replace(/%ID%/, testId);
  const testInterval = 100000000; // Just needs to be longer than the test.

  Services.prefs.clearUserPref(testPref);
  gUpdateTimerManager.registerTimer(
    testId,
    {},
    testInterval,
    true /* skipFirst */
  );
  let prefValue = Services.prefs.getIntPref(testPref, 0);
  Assert.notEqual(
    prefValue,
    0,
    "Last update time for test timer must not be 0."
  );
  let nowSeconds = Date.now() / 1000; // update timer lastUpdate prefs are set in seconds.
  Assert.ok(
    Math.abs(nowSeconds - prefValue) < 2,
    "Last update time for test timer must be now-ish."
  );

  gUpdateTimerManager.unregisterTimer(testId);
});
