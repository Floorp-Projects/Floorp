/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.sys.mjs",
});

const { ctypes } = ChromeUtils.importESModule(
  "resource://gre/modules/ctypes.sys.mjs"
);

// Derived from functionality in js/src/devtools/rootAnalysis/utility.js
function openLibrary(names) {
  for (const name of names) {
    try {
      return ctypes.open(name);
    } catch (e) {}
  }
  return undefined;
}

// Derived heavily from equivalent sandbox testing code.
// For more details see:
// https://searchfox.org/mozilla-central/rev/1aaacaeb4fa3aca6837ecc157e43e947229ba8ce/security/sandbox/test/browser_content_sandbox_utils.js#89
function raiseSignal(pid, sig) {
  try {
    const libc = openLibrary([
      "libc.so.6",
      "libc.so",
      "libc.dylib",
      "libSystem.B.dylib",
    ]);
    if (!libc) {
      info("Failed to open any libc shared object");
      return { ok: false };
    }

    // c.f. https://man7.org/linux/man-pages/man2/kill.2.html
    // This choice of typing for `pid` is complex, and brittle, as it's
    // platform dependent. Getting it wrong can result in incoreect
    //  generation/calling of the `kill` function. Unfortunately, as it's
    // defined as `pid_t` in a header, we can't easily get access to it.
    // For now, we just use an integer, and hope that the system int size
    // aligns with the `pid_t` size.
    const kill = libc.declare(
      "kill",
      ctypes.default_abi,
      ctypes.int, // return value
      ctypes.int32_t, // pid
      ctypes.int // sig
    );

    let kres = kill(pid, sig);
    if (kres != 0) {
      info(`Kill returned a non-zero result ${kres}.`);
      return { ok: false };
    }

    libc.close();
  } catch (e) {
    info(`Exception ${e} thrown while trying to call kill`);
    return { ok: false };
  }

  return { ok: true };
}

// We would like to use the following to wait for a stop signal to actually be
// handled:
//  await Services.profiler.waitOnePeriodicSampling();
// However, as we are trying to shut down the profiler using the sampler
// thread, this can cause complications between the callback waiting for the
// sampling to be over, and the sampler thread actually finishing.
// Instead, we use the BrowserTestUtils.waitForCondition to wait until the
// profiler is no longer active.
async function waitUntilProfilerStopped(interval = 1000, maxTries = 100) {
  await BrowserTestUtils.waitForCondition(
    () => !Services.profiler.IsActive(),
    "the profiler should be inactive",
    interval,
    maxTries
  );
}

async function cleanupAfterTest() {
  // We need to cleanup written profiles after a test
  // Get the system downloads directory, and use it to build a profile file
  let profile = FileUtils.File(await Downloads.getSystemDownloadsDirectory());

  // Get the process ID
  let pid = Services.appinfo.processID;

  // write it to the profile file name
  profile.append(`profile_0_${pid}.json`);

  // remove the file!
  await IOUtils.remove(profile.path, { ignoreAbsent: true });

  // Make sure the profiler is fully stopped, even if the test failed
  await Services.profiler.StopProfiler();
}

// Hardcode the constants SIGUSR1 and SIGUSR2.
// This is an absolutely terrible idea, as they are implementation defined!
// However, it turns out that for 99% of the platforms we care about, and for
// 99.999% of the platforms we test, these constants are, well, constant.
// Additionally, these constants are only for _testing_ the signal handling
// feature - the actual feature relies on platform specific definitions. This
//  may cause a mismatch if we test on on, say, a gnu hurd kernel, or on a
// linux kernel running on sparc, but the feature will not break - only
// the testing.
// const SIGUSR1 = Services.appinfo.OS === "Darwin" ? 30 : 10;
const SIGUSR2 = Services.appinfo.OS === "Darwin" ? 31 : 12;

add_task(async () => {
  info("Test that stopping the profiler with a posix signal works.");
  registerCleanupFunction(cleanupAfterTest);

  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler should not begin the test active."
  );

  const entries = 100;
  const interval = 1;
  const threads = [];
  const features = [];

  // Start the profiler, and ensure that it's active
  await Services.profiler.StartProfiler(entries, interval, threads, features);
  Assert.ok(Services.profiler.IsActive(), "The profiler should now be active.");

  // Get the process ID
  let pid = Services.appinfo.processID;

  // Try and stop the profiler using a signal.
  let result = raiseSignal(pid, SIGUSR2);
  Assert.ok(result, "Raising a SIGUSR2 signal should succeed.");

  await waitUntilProfilerStopped();

  do_test_finished();
});

add_task(async () => {
  info(
    "Test that stopping the profiler with a posix signal writes a profile file to the system download directory."
  );
  registerCleanupFunction(cleanupAfterTest);

  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler should not begin the test active."
  );

  const entries = 100;
  const interval = 1;
  const threads = [];
  const features = [];

  // Get the system downloads directory, and use it to build a profile file
  let profile = FileUtils.File(await Downloads.getSystemDownloadsDirectory());

  // Get the process ID
  let pid = Services.appinfo.processID;

  // use the pid to construct the name of the profile, and resulting file
  profile.append(`profile_0_${pid}.json`);

  // Start the profiler, and ensure that it's active
  await Services.profiler.StartProfiler(entries, interval, threads, features);
  Assert.ok(Services.profiler.IsActive(), "The profiler should now be active.");

  // Try and stop the profiler using a signal.
  let result = raiseSignal(pid, SIGUSR2);
  Assert.ok(result, "Raising a SIGUSR2 signal should succeed.");

  // Wait for the file to exist
  await BrowserTestUtils.waitForCondition(
    async () => await IOUtils.exists(profile.path),
    "Waiting for a profile file to be written to disk."
  );

  await waitUntilProfilerStopped();
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler should now be inactive."
  );

  do_test_finished();
});
