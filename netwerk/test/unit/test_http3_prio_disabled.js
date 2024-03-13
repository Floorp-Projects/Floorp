/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// this test file can be run directly as a part of parent/main process
// or indirectly from the wrapper test file as a part of child/content process

// need to get access to helper functions/structures
// load ensures
// * testing environment is available (ie Assert.ok())
/*global inChildProcess, test_flag_priority */
load("../unit/test_http3_prio_helpers.js");

// direct call to this test file should cleanup after itself
// otherwise the wrapper will handle
if (!inChildProcess()) {
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("network.http.http3.priority");
    Services.prefs.clearUserPref("network.http.priority_header.enabled");
    http3_clear_prefs();
  });
}

// setup once, before tests
add_task(async function setup() {
  // wrapper handles when testing as content process for pref change
  if (!inChildProcess()) {
    await http3_setup_tests("h3-29");
  }
});

// tests various flags when priority has been disabled on variable incremental
// this function should only be called the preferences priority disabled
async function test_http3_prio_disabled(incremental) {
  await test_flag_priority("disabled (none)", null, null, null, null); // default-test
  await test_flag_priority(
    "disabled (urgent_start)",
    Ci.nsIClassOfService.UrgentStart,
    null,
    incremental,
    null
  );
  await test_flag_priority(
    "disabled (leader)",
    Ci.nsIClassOfService.Leader,
    null,
    incremental,
    null
  );
  await test_flag_priority(
    "disabled (unblocked)",
    Ci.nsIClassOfService.Unblocked,
    null,
    incremental,
    null
  );
  await test_flag_priority(
    "disabled (follower)",
    Ci.nsIClassOfService.Follower,
    null,
    incremental,
    null
  );
  await test_flag_priority(
    "disabled (speculative)",
    Ci.nsIClassOfService.Speculative,
    null,
    incremental,
    null
  );
  await test_flag_priority(
    "disabled (background)",
    Ci.nsIClassOfService.Background,
    null,
    incremental,
    null
  );
  await test_flag_priority(
    "disabled (tail)",
    Ci.nsIClassOfService.Tail,
    null,
    incremental,
    null
  );
}

// run tests after setup

// test that various urgency flags and incremental=true don't propogate to header
// when priority setting is disabled
add_task(async function test_http3_prio_disabled_inc_true() {
  // wrapper handles when testing as content process for pref change
  if (!inChildProcess()) {
    Services.prefs.setBoolPref("network.http.http3.priority", false);
    Services.prefs.setBoolPref("network.http.priority_header.enabled", false);
  }
  await test_http3_prio_disabled(true);
});

// test that various urgency flags and incremental=false don't propogate to header
// when priority setting is disabled
add_task(async function test_http3_prio_disabled_inc_false() {
  // wrapper handles when testing as content process for pref change
  if (!inChildProcess()) {
    Services.prefs.setBoolPref("network.http.http3.priority", false);
    Services.prefs.setBoolPref("network.http.priority_header.enabled", false);
  }
  await test_http3_prio_disabled(false);
});
