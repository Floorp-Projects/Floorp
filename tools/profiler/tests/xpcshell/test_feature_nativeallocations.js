/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async () => {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }

  if (!Services.profiler.GetFeatures().includes("nativeallocations")) {
    Assert.ok(
      true,
      "Native allocations are not supported by this build, " +
        "skip run the rest of the test."
    );
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
      // Only instrument the main thread.
      threads: ["GeckoMain"],
      features: ["threads", "leaf", "nativeallocations"],
    });

    info(
      "Do some JS work for a little bit. This will increase the amount of allocations " +
        "that take place."
    );
    doWork();

    info("Get the profile data and analyze it.");
    const profile = await stopAndGetProfile();

    const {
      allocationPayloads,
      unmatchedAllocations,
      logAllocationsAndDeallocations,
    } = getAllocationInformation(profile);

    Assert.greater(
      allocationPayloads.length,
      0,
      "Native allocation payloads were recorded for the parent process' main thread when " +
        "the Native Allocation feature was turned on."
    );

    if (unmatchedAllocations.length !== 0) {
      info(
        "There were unmatched allocations. Log all of the allocations and " +
          "deallocations in order to aid debugging."
      );
      logAllocationsAndDeallocations();
      ok(
        false,
        "Found a deallocation that did not have a matching allocation site. " +
          "This could happen if balanced allocations is broken, or if the the " +
          "buffer size of this test was too small, and some markers ended up " +
          "rolling off."
      );
    }

    ok(true, "All deallocation sites had matching allocations.");
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

/**
 * Extract the allocation payloads, and find the unmatched allocations.
 */
function getAllocationInformation(profile) {
  // Get all of the allocation payloads.
  const allocationPayloads = getPayloadsOfType(
    profile.threads[0],
    "Native allocation"
  );

  // Decide what is an allocation and deallocation.
  const allocations = allocationPayloads.filter(
    payload => ensureIsNumber(payload.size) >= 0
  );
  const deallocations = allocationPayloads.filter(
    payload => ensureIsNumber(payload.size) < 0
  );

  // Now determine the unmatched allocations by building a set
  const allocationSites = new Set(
    allocations.map(({ memoryAddress }) => memoryAddress)
  );

  const unmatchedAllocations = deallocations.filter(
    ({ memoryAddress }) => !allocationSites.has(memoryAddress)
  );

  // Provide a helper to log out the allocations and deallocations on failure.
  function logAllocationsAndDeallocations() {
    for (const { memoryAddress } of allocations) {
      console.log("Allocations", formatHex(memoryAddress));
      allocationSites.add(memoryAddress);
    }

    for (const { memoryAddress } of deallocations) {
      console.log("Deallocations", formatHex(memoryAddress));
    }

    for (const { memoryAddress } of unmatchedAllocations) {
      console.log("Deallocation with no allocation", formatHex(memoryAddress));
    }
  }

  return {
    allocationPayloads,
    unmatchedAllocations,
    logAllocationsAndDeallocations,
  };
}

function ensureIsNumber(value) {
  if (typeof value !== "number") {
    throw new Error(`Expected a number: ${value}`);
  }
  return value;
}

function formatHex(number) {
  return `0x${number.toString(16)}`;
}
