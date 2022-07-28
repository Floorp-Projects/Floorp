/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that Java capturing works as expected.
 */
add_task(async () => {
  info("Test that Android Java sampler works as expected.");
  const entries = 10000;
  const interval = 1;
  const threads = [];
  const features = ["java"];

  Services.profiler.StartProfiler(entries, interval, features, threads);
  Assert.ok(Services.profiler.IsActive());

  await captureAtLeastOneJsSample();

  info(
    "Stop the profiler and check that we have successfully captured a profile" +
      " with the AndroidUI thread."
  );
  const profile = await stopNowAndGetProfile();
  Assert.notEqual(profile, null);
  const androidUiThread = profile.threads.find(
    thread => thread.name == "AndroidUI (JVM)"
  );
  Assert.notEqual(androidUiThread, null);
  Assert.ok(!Services.profiler.IsActive());
});
