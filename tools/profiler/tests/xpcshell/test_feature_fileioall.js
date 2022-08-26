/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is only needed for getting a temp file location, which IOUtils
// cannot currently do.
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

add_task(async () => {
  info(
    "Test that off-main thread fileio is captured for a profiled thread, " +
      "and that it will be sent to the main thread."
  );
  const filename = "test_marker_fileio";
  const profile = await startProfilerAndTriggerFileIO({
    features: ["fileioall"],
    threadsFilter: ["GeckoMain", "BgIOThreadPool"],
    filename,
  });

  const threads = getThreads(profile);
  const mainThread = threads.find(thread => thread.name === "GeckoMain");
  const mainThreadFileIO = getInflatedFileIOMarkers(mainThread, filename);
  let backgroundThread;
  let backgroundThreadFileIO;
  for (const thread of threads) {
    // Check for FileIO in any of the background threads.
    if (thread.name.startsWith("BgIOThreadPool")) {
      const markers = getInflatedFileIOMarkers(thread, filename);
      if (markers.length) {
        backgroundThread = thread;
        backgroundThreadFileIO = markers;
        break;
      }
    }
  }

  info("Check all of the main thread FileIO markers.");
  checkInflatedFileIOMarkers(mainThreadFileIO, filename);
  for (const { data, name } of mainThreadFileIO) {
    equal(
      name,
      "FileIO (non-main thread)",
      "The markers from off main thread are labeled as such."
    );
    equal(
      data.threadId,
      backgroundThread.tid,
      "The main thread FileIO markers were all sent from the background thread."
    );
  }

  info("Check all of the background thread FileIO markers.");
  checkInflatedFileIOMarkers(backgroundThreadFileIO, filename);
  for (const { data, name } of backgroundThreadFileIO) {
    equal(
      name,
      "FileIO",
      "The markers on the thread where they were generated just say FileIO"
    );
    equal(
      data.threadId,
      undefined,
      "The background thread FileIO correctly excludes the threadId."
    );
  }
});

add_task(async () => {
  info(
    "Test that off-main thread fileio is captured for a thread that is not profiled, " +
      "and that it will be sent to the main thread."
  );
  const filename = "test_marker_fileio";
  const profile = await startProfilerAndTriggerFileIO({
    features: ["fileioall"],
    threadsFilter: ["GeckoMain"],
    filename,
  });

  const threads = getThreads(profile);
  const mainThread = threads.find(thread => thread.name === "GeckoMain");
  const mainThreadFileIO = getInflatedFileIOMarkers(mainThread, filename);

  info("Check all of the main thread FileIO markers.");
  checkInflatedFileIOMarkers(mainThreadFileIO, filename);
  for (const { data, name } of mainThreadFileIO) {
    equal(
      name,
      "FileIO (non-profiled thread)",
      "The markers from off main thread are labeled as such."
    );
    equal(typeof data.threadId, "number", "A thread ID is captured.");
  }
});

/**
 * @typedef {Object} TestConfig
 * @prop {Array} features The list of profiler features
 * @prop {string[]} threadsFilter The list of threads to profile
 * @prop {string} filename A filename to trigger a write operation
 */

/**
 * Start the profiler and get FileIO markers.
 * @param {TestConfig}
 * @returns {Profile}
 */
async function startProfilerAndTriggerFileIO({
  features,
  threadsFilter,
  filename,
}) {
  const entries = 10000;
  const interval = 10;
  await Services.profiler.StartProfiler(
    entries,
    interval,
    features,
    threadsFilter
  );

  const tmpDir = OS.Constants.Path.tmpDir;
  const path = OS.Path.join(tmpDir, filename);

  info(`Using a temporary file to test FileIO: ${path}`);

  if (fileExists(path)) {
    console.warn(
      "This test is triggering FileIO by writing to a file. However, the test found an " +
        "existing file at the location it was trying to write to. This could happen " +
        "because a previous run of the test failed to clean up after itself. This test " +
        " will now clean up that file before running the test again."
    );
    await removeFile(path);
  }

  info("Write to the file, but do so using a background thread.");

  // IOUtils handles file operations using a background thread.
  await IOUtils.write(path, new TextEncoder().encode("Test data."));
  const exists = await fileExists(path);
  ok(exists, `Created temporary file at: ${path}`);

  info("Remove the file");
  await removeFile(path);

  return stopNowAndGetProfile();
}

async function fileExists(file) {
  try {
    let { type } = await IOUtils.stat(file);
    return type === "regular";
  } catch (_error) {
    return false;
  }
}

async function removeFile(file) {
  await IOUtils.remove(file);
  const exists = await fileExists(file);
  ok(!exists, `Removed temporary file: ${file}`);
}
