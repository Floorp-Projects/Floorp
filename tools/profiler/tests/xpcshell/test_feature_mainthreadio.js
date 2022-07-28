/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);

/**
 * Test that the IOInterposer is working correctly to capture main thread IO.
 *
 * This test should not run on release or beta, as the IOInterposer is wrapped in
 * an ifdef.
 */
add_task(async () => {
  {
    const filename = "profiler-mainthreadio-test-firstrun";
    const { markers, schema } = await runProfilerWithFileIO(
      ["mainthreadio"],
      filename
    );
    info("Check the FileIO markers when using the mainthreadio feature");
    checkInflatedFileIOMarkers(markers, filename);

    checkSchema(schema, {
      name: "FileIO",
      display: ["marker-chart", "marker-table", "timeline-fileio"],
      data: [
        {
          key: "operation",
          label: "Operation",
          format: "string",
          searchable: true,
        },
        { key: "source", label: "Source", format: "string", searchable: true },
        {
          key: "filename",
          label: "Filename",
          format: "file-path",
          searchable: true,
        },
      ],
    });
  }

  {
    const filename = "profiler-mainthreadio-test-no-instrumentation";
    const { markers } = await runProfilerWithFileIO([], filename);
    equal(
      markers.length,
      0,
      "No FileIO markers are found when the mainthreadio feature is not turned on " +
        "in the profiler."
    );
  }

  {
    const filename = "profiler-mainthreadio-test-secondrun";
    const { markers } = await runProfilerWithFileIO(["mainthreadio"], filename);
    info("Check the FileIO markers when re-starting the mainthreadio feature");
    checkInflatedFileIOMarkers(markers, filename);
  }
});

/**
 * Start the profiler and get FileIO markers and schema.
 *
 * @param {Array} features The list of profiler features
 * @param {string} filename A filename to trigger a write operation
 * @returns {{
 *   markers: InflatedMarkers[];
 *   schema: MarkerSchema;
 * }}
 */
async function runProfilerWithFileIO(features, filename) {
  const entries = 10000;
  const interval = 10;
  const threads = [];
  await Services.profiler.StartProfiler(entries, interval, features, threads);

  info("Get the file");
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

  info(
    "Generate file IO on the main thread using FileUtils.openSafeFileOutputStream."
  );
  const outputStream = FileUtils.openSafeFileOutputStream(file);

  const data = "Test data.";
  info("Write to the file");
  outputStream.write(data, data.length);

  info("Close the file");
  FileUtils.closeSafeFileOutputStream(outputStream);

  info("Remove the file");
  file.remove(false);

  const profile = await stopNowAndGetProfile();
  const mainThread = profile.threads.find(({ name }) => name === "GeckoMain");

  const schema = getSchema(profile, "FileIO");

  const markers = getInflatedFileIOMarkers(mainThread, filename);

  return { schema, markers };
}
