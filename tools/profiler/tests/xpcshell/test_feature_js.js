/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that JS capturing works as expected.
 */
add_task(async () => {
  const entries = 10000;
  const interval = 1;
  const threads = [];
  const features = ["js"];

  await Services.profiler.StartProfiler(entries, interval, features, threads);

  // Call the following to get a nice stack in the profiler:
  // functionA -> functionB -> functionC -> captureAtLeastOneJsSample
  const sampleIndex = await functionA();

  const profile = await stopNowAndGetProfile();

  const [thread] = profile.threads;
  const { samples } = thread;

  const inflatedStackFrames = getInflatedStackLocations(
    thread,
    samples.data[sampleIndex]
  );

  expectStackToContain(
    inflatedStackFrames,
    [
      "(root)",
      "js::RunScript",
      // The following regexes match a string similar to:
      //
      // "functionA (/gecko/obj/_tests/xpcshell/tools/profiler/tests/xpcshell/test_feature_js.js:47:0)"
      // or
      // "functionA (test_feature_js.js:47:0)"
      //
      //          this matches the script location
      //          |                       match the line number
      //          |                       |   match the column number
      //          v                       v   v
      /^functionA \(.*test_feature_js\.js:\d+:\d+\)$/,
      /^functionB \(.*test_feature_js\.js:\d+:\d+\)$/,
      /^functionC \(.*test_feature_js\.js:\d+:\d+\)$/,
    ],
    "The stack contains a few frame labels, as well as the JS functions that we called."
  );
});

function functionA() {
  return functionB();
}

function functionB() {
  return functionC();
}

async function functionC() {
  return captureAtLeastOneJsSample();
}
