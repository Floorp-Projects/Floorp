/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {FileUtils} = ChromeUtils.import("resource://gre/modules/FileUtils.jsm");

/**
 * Test that the IOInterposer is working correctly to capture main thread IO.
 *
 * This test should not run on release or beta, as the IOInterposer is wrapped in
 * an ifdef.
 */
add_task(async () => {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }

  // Let the test harness settle, in order to avoid extraneous FileIO operations. This
  // helps avoid false positives that we are actually triggering FileIO.
  await wait(10);

  {
    const filename = "profiler-mainthreadio-test-firstrun";
    const payloads = await startProfilerAndgetFileIOPayloads(["mainthreadio"], filename);

    greater(payloads.length, 0,
       "FileIO markers were found when using the mainthreadio feature on the profiler.");

    // It would be better to check on the filename, but Linux does not currently include
    // it. See https://bugzilla.mozilla.org/show_bug.cgi?id=1533531
    // ok(hasWritePayload(payloads, filename),
    //    "A FileIO marker is found when using the mainthreadio feature on the profiler.");
  }

  {
    const filename = "profiler-mainthreadio-test-no-instrumentation";
    const payloads = await startProfilerAndgetFileIOPayloads([], filename);

    equal(payloads.length, 0,
          "No FileIO markers are found when the mainthreadio feature is not turned on " +
          "in the profiler.");
  }

  {
    const filename = "profiler-mainthreadio-test-secondrun";
    const payloads = await startProfilerAndgetFileIOPayloads(["mainthreadio"], filename);

    greater(payloads.length, 0,
       "FileIO markers were found when re-starting the mainthreadio feature on the " +
       "profiler.");
    // It would be better to check on the filename, but Linux does not currently include
    // it. See https://bugzilla.mozilla.org/show_bug.cgi?id=1533531
    // ok(hasWritePayload(payloads, filename),
    //    "Re-enabling the mainthreadio re-installs the interposer, and we can capture " +
    //    "another FileIO payload.");
  }
});

/**
 * Start the profiler and get FileIO payloads.
 * @param {Array} features The list of profiler features
 * @param {string} filename A filename to trigger a write operation
 */
async function startProfilerAndgetFileIOPayloads(features, filename) {
  const entries = 10000;
  const interval = 10;
  const threads = [];
  Services.profiler.StartProfiler(entries, interval, features, features.length,
                                  threads, threads.length);


  const file = FileUtils.getFile("TmpD", [filename]);
  if (file.exists()) {
    console.warn(
      "This test is triggering FileIO by writing to a file. However, the test found an " +
      "existing file at the location it was trying to write to. This could happen " +
      "because a previous run of the test failed to clean up after itself. This test " +
      " will now clean up that file before running the test again."
    );
    file.remove(false);
  }

  const outputStream = FileUtils.openSafeFileOutputStream(file);

  const data = "Test data.";
  outputStream.write(data, data.length);
  FileUtils.closeSafeFileOutputStream(outputStream);

  file.remove(false);

  // Wait for the profiler to collect a sample.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1529053
  await wait(500);

  // Pause the profiler as we don't need to collect more samples as we retrieve
  // and serialize the profile.
  Services.profiler.PauseSampling();

  const profile = await Services.profiler.getProfileDataAsync();
  Services.profiler.StopProfiler();
  return getAllPayloadsOfType(profile, "FileIO");
}

/**
 * See if a list of payloads has a write operation from a file.
 *
 * @param {Array<Object>} payloads The payloads captured from the profiler.
 * @param {string} filename The filename used to test a write operation.
 */
function hasWritePayload(payloads, filename) {
  return payloads.some(payload => payload.filename.endsWith(filename));
}
