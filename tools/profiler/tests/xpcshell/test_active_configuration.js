/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async () => {
  info(
    "Checking that the profiler can fetch the information about the active " +
      "configuration that is being used to power the profiler."
  );

  equal(
    Services.profiler.activeConfiguration,
    null,
    "When the profile is off, there is no active configuration."
  );

  {
    info("Start the profiler.");
    const entries = 10000;
    const interval = 1;
    const threads = ["GeckoMain"];
    const features = ["js"];
    const activeTabID = 123;
    await Services.profiler.StartProfiler(
      entries,
      interval,
      features,
      threads,
      activeTabID
    );

    info("Generate the activeConfiguration.");
    const { activeConfiguration } = Services.profiler;
    const expectedConfiguration = {
      interval,
      threads,
      features,
      activeTabID,
      // The buffer is created as a power of two that can fit all of the entires
      // into it. If the ratio of entries to buffer size ever changes, this setting
      // will need to be updated.
      capacity: Math.pow(2, 14),
    };

    deepEqual(
      activeConfiguration,
      expectedConfiguration,
      "The active configuration matches configuration given."
    );

    info("Get the profile.");
    const profile = Services.profiler.getProfileData();
    deepEqual(
      profile.meta.configuration,
      expectedConfiguration,
      "The configuration also matches on the profile meta object."
    );
  }

  {
    const entries = 20000;
    const interval = 0.5;
    const threads = ["GeckoMain", "DOM Worker"];
    const features = [];
    const activeTabID = 111;
    const duration = 20;

    info("Restart the profiler with a new configuration.");
    await Services.profiler.StartProfiler(
      entries,
      interval,
      features,
      threads,
      activeTabID,
      // Also start it with duration, this property is optional.
      duration
    );

    info("Generate the activeConfiguration.");
    const { activeConfiguration } = Services.profiler;
    const expectedConfiguration = {
      interval,
      threads,
      features,
      activeTabID,
      duration,
      // The buffer is created as a power of two that can fit all of the entires
      // into it. If the ratio of entries to buffer size ever changes, this setting
      // will need to be updated.
      capacity: Math.pow(2, 15),
    };

    deepEqual(
      activeConfiguration,
      expectedConfiguration,
      "The active configuration matches the new configuration."
    );

    info("Get the profile.");
    const profile = Services.profiler.getProfileData();
    deepEqual(
      profile.meta.configuration,
      expectedConfiguration,
      "The configuration also matches on the profile meta object."
    );
  }

  await Services.profiler.StopProfiler();

  equal(
    Services.profiler.activeConfiguration,
    null,
    "When the profile is off, there is no active configuration."
  );
});
