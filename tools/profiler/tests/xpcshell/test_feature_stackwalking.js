/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Do a basic test to see if native frames are being collected for stackwalking. This
 * test is fairly naive, as it does not attempt to check that these are valid symbols,
 * only that some kind of stack walking is happening. It does this by making sure at
 * least two native frames are collected.
 */
add_task(async () => {
  const entries = 10000;
  const interval = 1;
  const threads = [];
  const features = ["stackwalk"];

  await Services.profiler.StartProfiler(entries, interval, features, threads);
  const sampleIndex = await captureAtLeastOneJsSample();

  const profile = await stopNowAndGetProfile();
  const [thread] = profile.threads;
  const { samples } = thread;

  const inflatedStackFrames = getInflatedStackLocations(
    thread,
    samples.data[sampleIndex]
  );
  const nativeStack = /^0x[0-9a-f]+$/;

  expectStackToContain(
    inflatedStackFrames,
    [
      "(root)",
      // There are probably more native stacks here.
      nativeStack,
      nativeStack,
      // Since this is an xpcshell test we know that JavaScript will run:
      "js::RunScript",
      // There are probably more native stacks here.
      nativeStack,
      nativeStack,
    ],
    "Expected native stacks to be interleaved between some frame labels. There should" +
      "be more than one native stack if stack walking is working correctly. There " +
      "is no attempt here to determine if the memory addresses point to the correct " +
      "symbols"
  );
});
