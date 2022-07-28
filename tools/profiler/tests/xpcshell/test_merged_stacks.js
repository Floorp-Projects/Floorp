/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that we correctly merge the three stack types, JS, native, and frame labels.
 */
add_task(async () => {
  const entries = 10000;
  const interval = 1;
  const threads = [];
  const features = ["js", "stackwalk"];

  await Services.profiler.StartProfiler(entries, interval, features, threads);

  // Call the following to get a nice stack in the profiler:
  // functionA -> functionB -> functionC
  const sampleIndex = await functionA();

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
      nativeStack,
      nativeStack,
      // There are more native stacks and frame labels here, but we know some execute
      // and then the "js::RunScript" frame label runs.
      "js::RunScript",
      nativeStack,
      nativeStack,
      // The following regexes match a string similar to:
      //
      // "functionA (/gecko/obj/_tests/xpcshell/tools/profiler/tests/xpcshell/test_merged_stacks.js:47:0)"
      // or
      // "functionA (test_merged_stacks.js:47:0)"
      //
      //          this matches the script location
      //          |                          match the line number
      //          |                          |   match the column number
      //          v                          v   v
      /^functionA \(.*test_merged_stacks\.js:\d+:\d+\)$/,
      /^functionB \(.*test_merged_stacks\.js:\d+:\d+\)$/,
      /^functionC \(.*test_merged_stacks\.js:\d+:\d+\)$/,
      // After the JS frames, then there are a bunch of arbitrary native stack frames
      // that run.
      nativeStack,
      nativeStack,
    ],
    "The stack contains a few frame labels, as well as the JS functions that we called."
  );
});

async function functionA() {
  return functionB();
}

async function functionB() {
  return functionC();
}

async function functionC() {
  return captureAtLeastOneJsSample();
}
