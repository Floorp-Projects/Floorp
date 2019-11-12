/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async () => {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  info(
    "Test that the profiler can install memory hooks and collect native allocation " +
      "information in the marker payloads."
  );
  {
    info("Start the profiler.");
    startProfiler({
      // Increase the entries so we don't overflow the buffer.
      entries: 1e8,
      // Only instrument the main thread.
      threads: ["GeckoMain"],
      features: ["threads", "leaf", "nativeallocations"],
    });

    info(
      "Do some JS work for a little bit. This will increase the amount of allocations " +
        "that take place."
    );
    doWork();

    info(
      "Go ahead and wait for a periodic sample as well, in order to make sure that " +
        "the native allocations play nicely with the rest of the profiler machinery."
    );
    await Services.profiler.waitOnePeriodicSampling();

    info("Get the profile data and analyze it.");
    const profile = await stopAndGetProfile();

    const allocationPayloads = getPayloadsOfType(
      profile.threads[0],
      "Native allocation"
    );

    Assert.greater(
      allocationPayloads.length,
      0,
      "Native allocation payloads were recorded for the parent process' main thread when " +
        "the Native Allocation feature was turned on."
    );
  }

  info("Restart the profiler, to ensure that we get no more allocations.");
  {
    startProfiler({ features: ["threads", "leaf"] });
    info("Do some work again.");
    doWork();
    info("Wait for the periodic sampling.");
    await Services.profiler.waitOnePeriodicSampling();

    const profile = await stopAndGetProfile();
    const allocationPayloads = getPayloadsOfType(
      profile.threads[0],
      "Native allocation"
    );

    Assert.equal(
      allocationPayloads.length,
      0,
      "No native allocations were collected when the feature was disabled."
    );
  }
});

function doWork() {
  this.n = 0;
  for (let i = 0; i < 1e5; i++) {
    this.n += Math.random();
  }
}
