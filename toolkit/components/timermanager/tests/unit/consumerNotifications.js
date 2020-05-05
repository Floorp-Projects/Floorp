/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Update Timer Manager Tests */

"use strict";

const Cm = Components.manager;

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const CATEGORY_UPDATE_TIMER = "update-timer";

const PREF_APP_UPDATE_TIMERMINIMUMDELAY = "app.update.timerMinimumDelay";
const PREF_APP_UPDATE_TIMERFIRSTINTERVAL = "app.update.timerFirstInterval";
const PREF_APP_UPDATE_LOG_ALL = "app.update.log.all";
const PREF_BRANCH_LAST_UPDATE_TIME = "app.update.lastUpdateTime.";

const MAIN_TIMER_INTERVAL = 1000; // milliseconds
const CONSUMER_TIMER_INTERVAL = 1; // seconds

const TESTS = [
  {
    desc: "Test Timer Callback 0",
    timerID: "test0-update-timer",
    defaultInterval: "bogus",
    prefInterval: "test0.timer.interval",
    contractID: "@mozilla.org/test0/timercallback;1",
    method: "createInstance",
    classID: Components.ID("9c7ce81f-98bb-4729-adb4-4d0deb0f59e5"),
    notified: false,
  },
  {
    desc: "Test Timer Callback 1",
    timerID: "test1-update-timer",
    defaultInterval: 86400,
    prefInterval: "test1.timer.interval",
    contractID: "@mozilla.org/test2/timercallback;1",
    method: "createInstance",
    classID: Components.ID("512834f3-05bb-46be-84e0-81d881a140b7"),
    notified: false,
  },
  {
    desc: "Test Timer Callback 2",
    timerID: "test2-update-timer",
    defaultInterval: CONSUMER_TIMER_INTERVAL,
    prefInterval: "test2.timer.interval",
    contractID: "@mozilla.org/test2/timercallback;1",
    method: "createInstance",
    classID: Components.ID("c8ac5027-8d11-4471-9d7c-fd692501b437"),
    notified: false,
  },
  {
    desc: "Test Timer Callback 3",
    timerID: "test3-update-timer",
    defaultInterval: CONSUMER_TIMER_INTERVAL,
    prefInterval: "test3.timer.interval",
    contractID: "@mozilla.org/test3/timercallback;1",
    method: "createInstance",
    classID: Components.ID("6b0e79f3-4ab8-414c-8f14-dde10e185727"),
    notified: false,
  },
  {
    desc: "Test Timer Callback 4",
    timerID: "test4-update-timer",
    defaultInterval: CONSUMER_TIMER_INTERVAL,
    prefInterval: "test4.timer.interval",
    contractID: "@mozilla.org/test4/timercallback;1",
    method: "createInstance",
    classID: Components.ID("2f6b7b92-e40f-4874-bfbb-eeb2412c959d"),
    notified: false,
  },
  {
    desc: "Test Timer Callback 5",
    timerID: "test5-update-timer",
    defaultInterval: 86400,
    prefInterval: "test5.timer.interval",
    contractID: "@mozilla.org/test5/timercallback;1",
    method: "createInstance",
    classID: Components.ID("8a95f611-b2ac-4c7e-8b73-9748c4839731"),
    notified: false,
  },
  {
    desc: "Test Timer Callback 6",
    timerID: "test6-update-timer",
    defaultInterval: CONSUMER_TIMER_INTERVAL,
    prefInterval: "test6.timer.interval",
    contractID: "@mozilla.org/test6/timercallback;1",
    method: "createInstance",
    classID: Components.ID("2d091020-e23c-11e2-a28f-0800200c9a66"),
    notified: false,
  },
  {
    desc: "Test Timer Callback 7",
    timerID: "test7-update-timer",
    defaultInterval: 86400,
    maxInterval: CONSUMER_TIMER_INTERVAL,
    prefInterval: "test7.timer.interval",
    contractID: "@mozilla.org/test7/timercallback;1",
    method: "createInstance",
    classID: Components.ID("8e8633ae-1d70-4a7a-8bea-6e1e6c5d7742"),
    notified: false,
  },
  {
    desc: "Test Timer Callback 8",
    timerID: "test8-update-timer",
    defaultInterval: CONSUMER_TIMER_INTERVAL,
    contractID: "@mozilla.org/test8/timercallback;1",
    classID: Components.ID("af878d4b-1d12-41f6-9a90-4e687367ecc1"),
    notified: false,
    lastUpdateTime: 0,
  },
  {
    desc: "Test Timer Callback 9",
    timerID: "test9-update-timer",
    defaultInterval: CONSUMER_TIMER_INTERVAL,
    contractID: "@mozilla.org/test9/timercallback;1",
    classID: Components.ID("5136b201-d64c-4328-8cf1-1a63491cc117"),
    notified: false,
    lastUpdateTime: 0,
  },
  {
    desc: "Test Timer Callback 10",
    timerID: "test10-update-timer",
    defaultInterval: CONSUMER_TIMER_INTERVAL,
    contractID: "@mozilla.org/test9/timercallback;1",
    classID: Components.ID("1f42bbb3-d116-4012-8491-3ec4797a97ee"),
    notified: false,
    lastUpdateTime: 0,
  },
];

var gUTM;
var gNextFunc;

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gPref",
  "@mozilla.org/preferences-service;1",
  "nsIPrefBranch"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gCatMan",
  "@mozilla.org/categorymanager;1",
  "nsICategoryManager"
);

XPCOMUtils.defineLazyGetter(this, "gCompReg", function() {
  return Cm.QueryInterface(Ci.nsIComponentRegistrar);
});

const gTest0TimerCallback = {
  notify: function T0CB_notify(aTimer) {
    // This can happen when another notification fails and this timer having
    // time to fire so check other timers are successful.
    do_throw("gTest0TimerCallback notify method should not have been called");
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback]),
};

const gTest0Factory = {
  createInstance: function T0F_createInstance(aOuter, aIID) {
    if (aOuter == null) {
      return gTest0TimerCallback.QueryInterface(aIID);
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
  },
};

const gTest1TimerCallback = {
  notify: function T1CB_notify(aTimer) {
    // This can happen when another notification fails and this timer having
    // time to fire so check other timers are successful.
    do_throw("gTest1TimerCallback notify method should not have been called");
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsITimer]),
};

const gTest1Factory = {
  createInstance: function T1F_createInstance(aOuter, aIID) {
    if (aOuter == null) {
      return gTest1TimerCallback.QueryInterface(aIID);
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
  },
};

const gTest2TimerCallback = {
  notify: function T2CB_notify(aTimer) {
    // This can happen when another notification fails and this timer having
    // time to fire so check other timers are successful.
    do_throw("gTest2TimerCallback notify method should not have been called");
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback]),
};

const gTest2Factory = {
  createInstance: function T2F_createInstance(aOuter, aIID) {
    if (aOuter == null) {
      return gTest2TimerCallback.QueryInterface(aIID);
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
  },
};

const gTest3TimerCallback = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback]),
};

const gTest3Factory = {
  createInstance: function T3F_createInstance(aOuter, aIID) {
    if (aOuter == null) {
      return gTest3TimerCallback.QueryInterface(aIID);
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
  },
};

const gTest4TimerCallback = {
  notify: function T4CB_notify(aTimer) {
    gCatMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, TESTS[4].desc, true);
    TESTS[4].notified = true;
    finished_test0thru7();
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback]),
};

const gTest4Factory = {
  createInstance: function T4F_createInstance(aOuter, aIID) {
    if (aOuter == null) {
      return gTest4TimerCallback.QueryInterface(aIID);
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
  },
};

const gTest5TimerCallback = {
  notify: function T5CB_notify(aTimer) {
    gCatMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, TESTS[5].desc, true);
    TESTS[5].notified = true;
    finished_test0thru7();
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback]),
};

const gTest5Factory = {
  createInstance: function T5F_createInstance(aOuter, aIID) {
    if (aOuter == null) {
      return gTest5TimerCallback.QueryInterface(aIID);
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
  },
};

const gTest6TimerCallback = {
  notify: function T6CB_notify(aTimer) {
    gCatMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, TESTS[6].desc, true);
    TESTS[6].notified = true;
    finished_test0thru7();
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback]),
};

const gTest6Factory = {
  createInstance: function T6F_createInstance(aOuter, aIID) {
    if (aOuter == null) {
      return gTest6TimerCallback.QueryInterface(aIID);
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
  },
};

const gTest7TimerCallback = {
  notify: function T7CB_notify(aTimer) {
    gCatMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, TESTS[7].desc, true);
    TESTS[7].notified = true;
    finished_test0thru7();
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback]),
};

const gTest7Factory = {
  createInstance: function T7F_createInstance(aOuter, aIID) {
    if (aOuter == null) {
      return gTest7TimerCallback.QueryInterface(aIID);
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
  },
};

const gTest8TimerCallback = {
  notify: function T8CB_notify(aTimer) {
    TESTS[8].notified = true;
    TESTS[8].notifyTime = Date.now();
    executeSoon(function() {
      check_test8thru10(gTest8TimerCallback);
    });
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback]),
};

const gTest8Factory = {
  createInstance: function T8F_createInstance(aOuter, aIID) {
    if (aOuter == null) {
      return gTest8TimerCallback.QueryInterface(aIID);
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
  },
};

const gTest9TimerCallback = {
  notify: function T9CB_notify(aTimer) {
    TESTS[9].notified = true;
    TESTS[9].notifyTime = Date.now();
    executeSoon(function() {
      check_test8thru10(gTest9TimerCallback);
    });
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback]),
};

const gTest10TimerCallback = {
  notify: function T9CB_notify(aTimer) {
    // The timer should have been unregistered before this could
    // be called.
    do_throw("gTest10TimerCallback notify method should not have been called");
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback]),
};

const gTest9Factory = {
  createInstance: function T9F_createInstance(aOuter, aIID) {
    if (aOuter == null) {
      return gTest9TimerCallback.QueryInterface(aIID);
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
  },
};

function run_test() {
  do_test_pending();

  // Set the timer to fire every second
  gPref.setIntPref(
    PREF_APP_UPDATE_TIMERMINIMUMDELAY,
    MAIN_TIMER_INTERVAL / 1000
  );
  gPref.setIntPref(PREF_APP_UPDATE_TIMERFIRSTINTERVAL, MAIN_TIMER_INTERVAL);
  gPref.setBoolPref(PREF_APP_UPDATE_LOG_ALL, true);

  // Remove existing update timers to prevent them from being notified
  for (let { data: entry } of gCatMan.enumerateCategory(
    CATEGORY_UPDATE_TIMER
  )) {
    gCatMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, entry, false);
  }

  gUTM = Cc["@mozilla.org/updates/timer-manager;1"]
    .getService(Ci.nsIUpdateTimerManager)
    .QueryInterface(Ci.nsIObserver);
  gUTM.observe(null, "utm-test-init", "");

  executeSoon(run_test0thru7);
}

function end_test() {
  gUTM.observe(null, "profile-before-change", "");
  do_test_finished();
}

function run_test0thru7() {
  gNextFunc = check_test0thru7;
  // bogus default interval
  gCompReg.registerFactory(
    TESTS[0].classID,
    TESTS[0].desc,
    TESTS[0].contractID,
    gTest0Factory
  );
  gCatMan.addCategoryEntry(
    CATEGORY_UPDATE_TIMER,
    TESTS[0].desc,
    [
      TESTS[0].contractID,
      TESTS[0].method,
      TESTS[0].timerID,
      TESTS[0].prefInterval,
      TESTS[0].defaultInterval,
    ].join(","),
    false,
    true
  );

  // doesn't implement nsITimerCallback
  gCompReg.registerFactory(
    TESTS[1].classID,
    TESTS[1].desc,
    TESTS[1].contractID,
    gTest1Factory
  );
  gCatMan.addCategoryEntry(
    CATEGORY_UPDATE_TIMER,
    TESTS[1].desc,
    [
      TESTS[1].contractID,
      TESTS[1].method,
      TESTS[1].timerID,
      TESTS[1].prefInterval,
      TESTS[1].defaultInterval,
    ].join(","),
    false,
    true
  );

  // has a last update time of now - 43200 which is half of its interval
  let lastUpdateTime = Math.round(Date.now() / 1000) - 43200;
  gPref.setIntPref(
    PREF_BRANCH_LAST_UPDATE_TIME + TESTS[2].timerID,
    lastUpdateTime
  );
  gCompReg.registerFactory(
    TESTS[2].classID,
    TESTS[2].desc,
    TESTS[2].contractID,
    gTest2Factory
  );
  gCatMan.addCategoryEntry(
    CATEGORY_UPDATE_TIMER,
    TESTS[2].desc,
    [
      TESTS[2].contractID,
      TESTS[2].method,
      TESTS[2].timerID,
      TESTS[2].prefInterval,
      TESTS[2].defaultInterval,
    ].join(","),
    false,
    true
  );

  // doesn't have a notify method
  gCompReg.registerFactory(
    TESTS[3].classID,
    TESTS[3].desc,
    TESTS[3].contractID,
    gTest3Factory
  );
  gCatMan.addCategoryEntry(
    CATEGORY_UPDATE_TIMER,
    TESTS[3].desc,
    [
      TESTS[3].contractID,
      TESTS[3].method,
      TESTS[3].timerID,
      TESTS[3].prefInterval,
      TESTS[3].defaultInterval,
    ].join(","),
    false,
    true
  );

  // already has a last update time
  gPref.setIntPref(PREF_BRANCH_LAST_UPDATE_TIME + TESTS[4].timerID, 1);
  gCompReg.registerFactory(
    TESTS[4].classID,
    TESTS[4].desc,
    TESTS[4].contractID,
    gTest4Factory
  );
  gCatMan.addCategoryEntry(
    CATEGORY_UPDATE_TIMER,
    TESTS[4].desc,
    [
      TESTS[4].contractID,
      TESTS[4].method,
      TESTS[4].timerID,
      TESTS[4].prefInterval,
      TESTS[4].defaultInterval,
    ].join(","),
    false,
    true
  );

  // has an interval preference that overrides the default
  gPref.setIntPref(TESTS[5].prefInterval, CONSUMER_TIMER_INTERVAL);
  gCompReg.registerFactory(
    TESTS[5].classID,
    TESTS[5].desc,
    TESTS[5].contractID,
    gTest5Factory
  );
  gCatMan.addCategoryEntry(
    CATEGORY_UPDATE_TIMER,
    TESTS[5].desc,
    [
      TESTS[5].contractID,
      TESTS[5].method,
      TESTS[5].timerID,
      TESTS[5].prefInterval,
      TESTS[5].defaultInterval,
    ].join(","),
    false,
    true
  );

  // has a next update time 24 hours from now
  let nextUpdateTime = Math.round(Date.now() / 1000) + 86400;
  gPref.setIntPref(
    PREF_BRANCH_LAST_UPDATE_TIME + TESTS[6].timerID,
    nextUpdateTime
  );
  gCompReg.registerFactory(
    TESTS[6].classID,
    TESTS[6].desc,
    TESTS[6].contractID,
    gTest6Factory
  );
  gCatMan.addCategoryEntry(
    CATEGORY_UPDATE_TIMER,
    TESTS[6].desc,
    [
      TESTS[6].contractID,
      TESTS[6].method,
      TESTS[6].timerID,
      TESTS[6].prefInterval,
      TESTS[6].defaultInterval,
    ].join(","),
    false,
    true
  );

  // has a maximum interval set by the value of MAIN_TIMER_INTERVAL
  gPref.setIntPref(TESTS[7].prefInterval, 86400);
  gCompReg.registerFactory(
    TESTS[7].classID,
    TESTS[7].desc,
    TESTS[7].contractID,
    gTest7Factory
  );
  gCatMan.addCategoryEntry(
    CATEGORY_UPDATE_TIMER,
    TESTS[7].desc,
    [
      TESTS[7].contractID,
      TESTS[7].method,
      TESTS[7].timerID,
      TESTS[7].prefInterval,
      TESTS[7].defaultInterval,
      TESTS[7].maxInterval,
    ].join(","),
    false,
    true
  );
}

function finished_test0thru7() {
  if (
    TESTS[4].notified &&
    TESTS[5].notified &&
    TESTS[6].notified &&
    TESTS[7].notified
  ) {
    executeSoon(gNextFunc);
  }
}

function check_test0thru7() {
  Assert.ok(
    !TESTS[0].notified,
    "a category registered timer didn't fire due to an invalid " +
      "default interval"
  );

  Assert.ok(
    !TESTS[1].notified,
    "a category registered timer didn't fire due to not implementing " +
      "nsITimerCallback"
  );

  Assert.ok(
    !TESTS[2].notified,
    "a category registered timer didn't fire due to the next update " +
      "time being in the future"
  );

  Assert.ok(
    !TESTS[3].notified,
    "a category registered timer didn't fire due to not having a " +
      "notify method"
  );

  Assert.ok(TESTS[4].notified, "a category registered timer has fired");

  Assert.ok(
    TESTS[5].notified,
    "a category registered timer fired that has an interval " +
      "preference that overrides a default that wouldn't have fired yet"
  );

  Assert.ok(
    TESTS[6].notified,
    "a category registered timer has fired due to the next update " +
      "time being reset due to a future last update time"
  );

  Assert.ok(
    gPref.prefHasUserValue(PREF_BRANCH_LAST_UPDATE_TIME + TESTS[4].timerID),
    "first of two category registered timers last update time has " +
      "a user value"
  );
  Assert.ok(
    gPref.prefHasUserValue(PREF_BRANCH_LAST_UPDATE_TIME + TESTS[5].timerID),
    "second of two category registered timers last update time has " +
      "a user value"
  );

  // Remove the category timers that should have failed
  gCatMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, TESTS[0].desc, true);
  gCatMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, TESTS[1].desc, true);
  gCatMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, TESTS[2].desc, true);
  gCatMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, TESTS[3].desc, true);
  for (let { data: entry } of gCatMan.enumerateCategory(
    CATEGORY_UPDATE_TIMER
  )) {
    gCatMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, entry, false);
  }

  let entries = gCatMan.enumerateCategory(CATEGORY_UPDATE_TIMER);
  Assert.ok(
    !entries.hasMoreElements(),
    "no " +
      CATEGORY_UPDATE_TIMER +
      " categories should still be " +
      "registered"
  );

  executeSoon(run_test8thru10);
}

function run_test8thru10() {
  gPref.setIntPref(PREF_BRANCH_LAST_UPDATE_TIME + TESTS[8].timerID, 1);
  gCompReg.registerFactory(
    TESTS[8].classID,
    TESTS[8].desc,
    TESTS[8].contractID,
    gTest8Factory
  );
  gUTM.registerTimer(
    TESTS[8].timerID,
    gTest8TimerCallback,
    TESTS[8].defaultInterval
  );
  gPref.setIntPref(PREF_BRANCH_LAST_UPDATE_TIME + TESTS[9].timerID, 1);
  gCompReg.registerFactory(
    TESTS[9].classID,
    TESTS[9].desc,
    TESTS[9].contractID,
    gTest9Factory
  );
  gUTM.registerTimer(
    TESTS[9].timerID,
    gTest9TimerCallback,
    TESTS[9].defaultInterval
  );
  gUTM.registerTimer(
    TESTS[10].timerID,
    gTest10TimerCallback,
    TESTS[10].defaultInterval
  );
  gUTM.unregisterTimer(TESTS[10].timerID);
}

function check_test8thru10(aTestTimerCallback) {
  aTestTimerCallback.timesCalled = (aTestTimerCallback.timesCalled || 0) + 1;
  if (aTestTimerCallback.timesCalled < 2) {
    return;
  }

  Assert.ok(
    TESTS[8].notified,
    "first registerTimer registered timer should have fired"
  );

  Assert.ok(
    TESTS[9].notified,
    "second registerTimer registered timer should have fired"
  );

  // Check that 'staggering' has happened: even though the two events wanted to fire at
  // the same time, we waited a full MAIN_TIMER_INTERVAL between them.
  // (to avoid sensitivity to random timing issues, we fudge by a factor of 0.5 here)
  Assert.ok(
    Math.abs(TESTS[8].notifyTime - TESTS[9].notifyTime) >=
      MAIN_TIMER_INTERVAL * 0.5,
    "staggering between two timers that want to fire at the same " +
      "time should have occured"
  );

  let time = gPref.getIntPref(PREF_BRANCH_LAST_UPDATE_TIME + TESTS[8].timerID);
  Assert.notEqual(
    time,
    1,
    "first registerTimer registered timer last update time " +
      "should have been updated"
  );

  time = gPref.getIntPref(PREF_BRANCH_LAST_UPDATE_TIME + TESTS[9].timerID);
  Assert.notEqual(
    time,
    1,
    "second registerTimer registered timer last update time " +
      "should have been updated"
  );

  end_test();
}
