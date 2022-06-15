/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  if (new Date().getHours() >= 3 && new Date().getHours() <= 4) {
    // We skip this test if it's too close to 4AM, when we might send a
    // "metrics" ping between init and this test being run.
    ok(true, "Too close to 'metrics' ping send window. Skipping test.");
    return;
  }
  Assert.greater(
    Glean.fog.initialization.testGetValue(),
    0,
    "FOG init happened, and its time was measured."
  );
});
