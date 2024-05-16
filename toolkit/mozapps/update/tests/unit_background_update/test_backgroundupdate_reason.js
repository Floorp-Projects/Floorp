/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et
 * this source code form is subject to the terms of the mozilla public license,
 * v. 2.0. if a copy of the mpl was not distributed with this file, you can
 * obtain one at http://mozilla.org/mpl/2.0/. */

"use strict";

const { BackgroundUpdate } = ChromeUtils.importESModule(
  "resource://gre/modules/BackgroundUpdate.sys.mjs"
);

add_setup(async function test_setup() {
  // These tests use per-installation prefs, and those are a shared resource, so
  // they require some non-trivial setup.
  setupTestCommon(null);
  await standardInit();

  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // We need to initialize it once, otherwise operations will be stuck in the
  // pre-init queue.
  Services.fog.initializeFOG();
});

// Because we want to use the keys from REASON as strings and send these with
// Glean, we have to make sure, that they meet the requirements for `String
// Lists` and are not too long.
add_task(async function test_reasons_length() {
  for (const key of Object.keys(BackgroundUpdate.REASON)) {
    Glean.backgroundUpdate.reasonsToNotUpdate.add(key);
    // No exception means success.
    Assert.ok(
      Array.isArray(Glean.backgroundUpdate.reasonsToNotUpdate.testGetValue()),
      "Glean allows the name of the reason to be '" + key + "'"
    );
  }
});

// The string list in Glean can overflow and has a hard limit of 20 entries.
// This test toggles a switch to reach this limit and fails if this causes an
// exception, because we want to avoid that statistical data collection can have
// an negative impact on the success rate of background updates.
add_task(async function test_reasons_overflow() {
  let prev = await UpdateUtils.getAppUpdateAutoEnabled();
  try {
    for (let i = 1; i <= 21; i++) {
      await UpdateUtils.setAppUpdateAutoEnabled(false);
      await BackgroundUpdate._reasonsToNotUpdateInstallation();
      await UpdateUtils.setAppUpdateAutoEnabled(true);
      await BackgroundUpdate._reasonsToNotUpdateInstallation();
      Assert.ok(true, "Overflow test successful for run #" + i);
    }
  } finally {
    ok(true, "resetting AppUpdateAutoEnabled to " + prev);
    await UpdateUtils.setAppUpdateAutoEnabled(prev);
  }
});

add_task(() => {
  // `setupTestCommon()` calls `do_test_pending()`; this calls
  // `do_test_finish()`.  The `add_task` schedules this to run after all the
  // other tests have completed.
  doTestFinish();
});
