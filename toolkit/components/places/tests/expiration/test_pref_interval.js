/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * What this is aimed to test:
 *
 * Expiration relies on an interval, that is user-preffable setting
 * "places.history.expiration.interval_seconds".
 * On pref change it will stop current interval timer and fire a new one,
 * that will obey the new value.
 * If the pref is set to a number <= 0 we will use the default value.
 */

// Default timer value for expiration in seconds.  Must have same value as
// PREF_INTERVAL_SECONDS_NOTSET in nsPlacesExpiration.
const DEFAULT_TIMER_DELAY_SECONDS = 3 * 60;

// Sync this with the const value in the component.
const EXPIRE_AGGRESSIVITY_MULTIPLIER = 3;

// Provide a mock timer implementation, so there is no need to wait seconds to
// achieve test results.
const Cm = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
const TIMER_CONTRACT_ID = "@mozilla.org/timer;1";
const kMockCID = Components.ID("52bdf457-4de3-48ae-bbbb-f3cbcbcad05f");

// Used to preserve the original timer factory.
let gOriginalCID = Cm.contractIDToCID(TIMER_CONTRACT_ID);

// The mock timer factory.
let gMockTimerFactory = {
  createInstance: function MTF_createInstance(aOuter, aIID) {
    if (aOuter != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return mockTimerImpl.QueryInterface(aIID);
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIFactory,
  ])
}

let mockTimerImpl = {
  initWithCallback: function MTI_initWithCallback(aCallback, aDelay, aType) {
    print("Checking timer delay equals expected interval value");
    if (!currentTest)
      return;
    // History status is not dirty, so the timer is delayed.
    do_check_eq(aDelay, currentTest.expectedTimerDelay * 1000 * EXPIRE_AGGRESSIVITY_MULTIPLIER)

    do_execute_soon(runNextTest);
  },

  cancel: function() {},
  initWithFuncCallback: function() {},
  init: function() {},

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsITimer,
  ])
}

function replace_timer_factory() {
  Cm.registerFactory(kMockCID,
                     "Mock timer",
                     TIMER_CONTRACT_ID,
                     gMockTimerFactory);
}

do_register_cleanup(function() {
  // Shutdown expiration before restoring original timer, otherwise we could
  // leak due to the different implementation.
  shutdownExpiration();

  // Restore original timer factory.
  Cm.unregisterFactory(kMockCID,
                       gMockTimerFactory);
  Cm.registerFactory(gOriginalCID,
                     "",
                     TIMER_CONTRACT_ID,
                     null);
});


let tests = [

  // This test should be the first, so the interval won't be influenced by
  // status of history.
  { desc: "Set interval to 1s.",
    interval: 1,
    expectedTimerDelay: 1
  },

  { desc: "Set interval to a negative value.",
    interval: -1,
    expectedTimerDelay: DEFAULT_TIMER_DELAY_SECONDS
  },

  { desc: "Set interval to 0.",
    interval: 0,
    expectedTimerDelay: DEFAULT_TIMER_DELAY_SECONDS
  },

  { desc: "Set interval to a large value.",
    interval: 100,
    expectedTimerDelay: 100
  },

];

let currentTest;

function run_test() {
  // The pref should not exist by default.
  try {
    getInterval();
    do_throw("interval pref should not exist by default");
  }
  catch (ex) {}

  // Use our own mock timer implementation.
  replace_timer_factory();

  // Force the component, so it will start observing preferences.
  force_expiration_start();

  runNextTest();
  do_test_pending();
}

function runNextTest() {
  if (tests.length) {
    currentTest = tests.shift();
    print(currentTest.desc);
    setInterval(currentTest.interval);
  }
  else {
    clearInterval();
    do_test_finished();
  }
}
