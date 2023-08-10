/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that the CPU Speed markers exist if and only if the cpufreq feature
 * is enabled.
 */
add_task(async () => {
  {
    Assert.ok(
      Services.profiler.GetAllFeatures().includes("cpufreq"),
      "the CPU Frequency feature should exist on all platforms."
    );
    const shouldBeEnabled = ["win", "linux", "android"].includes(
      AppConstants.platform
    );
    Assert.equal(
      Services.profiler.GetFeatures().includes("cpufreq"),
      shouldBeEnabled,
      "the CPU Frequency feature should only be enabled on some platforms."
    );

    if (!shouldBeEnabled) {
      return;
    }
  }

  {
    const { markers, schema } = await runProfilerWithCPUSpeed(["cpufreq"]);

    checkSchema(schema, {
      name: "CPUSpeed",
      tableLabel: "{marker.name} Speed = {marker.data.speed}GHz",
      display: ["marker-chart", "marker-table"],
      data: [
        {
          key: "speed",
          label: "CPU Speed (GHz)",
          format: "string",
        },
      ],
      graphs: [
        {
          key: "speed",
          type: "bar",
          color: "ink",
        },
      ],
    });

    Assert.greater(markers.length, 0, "There should be CPU Speed markers");
    const names = new Set();
    for (let marker of markers) {
      names.add(marker.name);
    }
    let processData = await Services.sysinfo.processInfo;
    equal(
      names.size,
      processData.count,
      "We have as many CPU Speed marker names as CPU cores on the machine"
    );
  }

  {
    const { markers } = await runProfilerWithCPUSpeed([]);
    equal(
      markers.length,
      0,
      "No CPUSpeed markers are found when the cpufreq feature is not turned on " +
        "in the profiler."
    );
  }
});

function getInflatedCPUFreqMarkers(thread) {
  const markers = getInflatedMarkerData(thread);
  return markers.filter(marker => marker.data?.type === "CPUSpeed");
}

/**
 * Start the profiler and get CPUSpeed markers and schema.
 *
 * @param {Array} features The list of profiler features
 * @returns {{
 *   markers: InflatedMarkers[];
 *   schema: MarkerSchema;
 * }}
 */
async function runProfilerWithCPUSpeed(features, filename) {
  const entries = 10000;
  const interval = 10;
  const threads = [];
  await Services.profiler.StartProfiler(entries, interval, features, threads);

  const profile = await waitSamplingAndStopAndGetProfile();
  const mainThread = profile.threads.find(({ name }) => name === "GeckoMain");

  const schema = getSchema(profile, "CPUSpeed");
  const markers = getInflatedCPUFreqMarkers(mainThread);
  return { schema, markers };
}
