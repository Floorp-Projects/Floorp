/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_markers_gc_cc() {
  info("Test GC&CC markers.");

  info("Create a throwaway profile.");
  await startProfiler({});
  let tempProfileContainer = { profile: null };
  tempProfileContainer.profile = await waitSamplingAndStopAndGetProfile();

  info("Restart the profiler.");
  await startProfiler({});

  info("Throw away the previous profile, which should be garbage-collected.");
  Assert.equal(
    typeof tempProfileContainer.profile,
    "object",
    "Previously-captured profile should be an object"
  );
  delete tempProfileContainer.profile;
  Assert.equal(
    typeof tempProfileContainer.profile,
    "undefined",
    "Deleted profile should now be undefined"
  );

  info("Force GC&CC");
  SpecialPowers.gc();
  SpecialPowers.forceShrinkingGC();
  SpecialPowers.forceCC();
  SpecialPowers.gc();
  SpecialPowers.forceShrinkingGC();
  SpecialPowers.forceCC();

  info("Stop the profiler and get the profile.");
  const profile = await waitSamplingAndStopAndGetProfile();

  const markers = getInflatedMarkerData(profile.threads[0]);
  Assert.ok(
    markers.some(({ data }) => data?.type === "GCSlice"),
    "A GCSlice marker was recorded"
  );
  Assert.ok(
    markers.some(({ data }) => data?.type === "CCSlice"),
    "A CCSlice marker was recorded"
  );
});
