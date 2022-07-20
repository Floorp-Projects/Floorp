/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

add_setup(
  /* on Android FOG is set up through head.js */
  { skip_if: () => AppConstants.platform == "android" },
  function test_setup() {
    // FOG needs a profile directory to put its data in.
    do_get_profile();

    // Glean init (via `chrono`) gets the timezone via unprotected write.
    // This is being worked around:
    // https://github.com/chronotope/chrono/pull/677
    // Until that reaches a release and we update to it (bug 1780401), ensure
    // local time has been loaded by JS before we kick of Glean init.
    new Date().getHours(); // used for its side effect.

    // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
    Services.fog.initializeFOG();
  }
);

add_task(function test_fog_init_works() {
  if (new Date().getHours() >= 3 && new Date().getHours() <= 4) {
    // We skip this test if it's too close to 4AM, when we might send a
    // "metrics" ping between init and this test being run.
    Assert.ok(true, "Too close to 'metrics' ping send window. Skipping test.");
    return;
  }
  Assert.greater(
    Glean.fog.initialization.testGetValue(),
    0,
    "FOG init happened, and its time was measured."
  );
});
