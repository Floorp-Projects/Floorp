/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/ObjectUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/UpdateUtils.jsm", this);

add_task(async function testHistogramPacking() {
  const HISTOGRAM_SNAPSHOT = {
    sample_process: {
      HISTOGRAM_1_DATA: {
        counts: [
          1, 0, 0
        ],
        ranges: [
          0, 1, 2
        ],
        max: 2,
        min: 1,
        sum: 1,
        histogram_type: 4
      },
      TELEMETRY_TEST_HISTOGRAM_2_DATA: {
        counts: [
          1, 0, 0
        ],
        ranges: [
          0, 1, 2
        ],
        max: 2,
        min: 1,
        sum: 1,
        histogram_type: 4
      },
    },
  };

  const HISTOGRAM_1_DATA = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 4,
    values: {
      "0": 1,
      "1": 0
    },
    sum: 1
  };

  const packedHistograms =
    TelemetryUtils.packHistograms(HISTOGRAM_SNAPSHOT, false /* testingMode */);

  // Check that it worked and that TELEMETRY_TEST_* got filtered.
  const EXPECTED_DATA =  {
    sample_process: {
      HISTOGRAM_1_DATA,
    }
  };
  Assert.ok(!("TELEMETRY_TEST_HISTOGRAM_2_DATA" in packedHistograms.sample_process),
            "Test histograms must not be reported outside of test mode.");
  Assert.ok(ObjectUtils.deepEqual(EXPECTED_DATA, packedHistograms),
            "Packed data must be correct.");

  // Do the same packing in testing mode.
  const packedHistogramsTest =
    TelemetryUtils.packHistograms(HISTOGRAM_SNAPSHOT, true /* testingMode */);

  // Check that TELEMETRY_TEST_* is there.
  const EXPECTED_DATA_TEST =  {
    sample_process: {
      HISTOGRAM_1_DATA,
      TELEMETRY_TEST_HISTOGRAM_2_DATA: HISTOGRAM_1_DATA
    }
  };
  Assert.ok("TELEMETRY_TEST_HISTOGRAM_2_DATA" in packedHistogramsTest.sample_process,
            "Test histograms must be reported in test mode.");
  Assert.ok(ObjectUtils.deepEqual(EXPECTED_DATA_TEST, packedHistogramsTest),
            "Packed data must be correct.");
});

add_task(async function testKeyedHistogramPacking() {
  const KEYED_HISTOGRAM_SNAPSHOT = {
    sample_process: {
      HISTOGRAM_1_DATA: {
        someKey: {
          counts: [
            1, 0, 0
          ],
          ranges: [
            0, 1, 2
          ],
          max: 2,
          min: 1,
          sum: 1,
          histogram_type: 4
        },
        otherKey: {
          counts: [
            1, 0, 0
          ],
          ranges: [
            0, 1, 2
          ],
          max: 2,
          min: 1,
          sum: 1,
          histogram_type: 4
        }
      },
      TELEMETRY_TEST_HISTOGRAM_2_DATA: {
        someKey: {
          counts: [
            1, 0, 0
          ],
          ranges: [
            0, 1, 2
          ],
          max: 2,
          min: 1,
          sum: 1,
          histogram_type: 4
        }
      },
    },
  };

  const someKey = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 4,
    values: {
      "0": 1,
      "1": 0
    },
    sum: 1
  };

  const packedKeyedHistograms =
    TelemetryUtils.packKeyedHistograms(KEYED_HISTOGRAM_SNAPSHOT, false /* testingMode */);

  // Check that it worked and that TELEMETRY_TEST_* got filtered.
  const EXPECTED_DATA =  {
    sample_process: {
      HISTOGRAM_1_DATA: {
        someKey,
        otherKey: someKey
      }
    }
  };
  Assert.ok(!("TELEMETRY_TEST_HISTOGRAM_2_DATA" in packedKeyedHistograms.sample_process),
            "Test histograms must not be reported outside of test mode.");
  Assert.ok(ObjectUtils.deepEqual(EXPECTED_DATA, packedKeyedHistograms),
            "Packed data must be correct.");

  // Do the same packing in testing mode.
  const packedKeyedHistogramsTest =
    TelemetryUtils.packKeyedHistograms(KEYED_HISTOGRAM_SNAPSHOT, true /* testingMode */);

  // Check that TELEMETRY_TEST_* is there.
  const EXPECTED_DATA_TEST =  {
    sample_process: {
      HISTOGRAM_1_DATA: {
        someKey,
        otherKey: someKey
      },
      TELEMETRY_TEST_HISTOGRAM_2_DATA: {
        someKey,
      }
    }
  };
  Assert.ok("TELEMETRY_TEST_HISTOGRAM_2_DATA" in packedKeyedHistogramsTest.sample_process,
            "Test histograms must be reported in test mode.");
  Assert.ok(ObjectUtils.deepEqual(EXPECTED_DATA_TEST, packedKeyedHistogramsTest),
            "Packed data must be correct.");
});

add_task(async function testUpdateChannelOverride() {
  if (Preferences.has(TelemetryUtils.Preferences.OverrideUpdateChannel)) {
    // If the pref is already set at this point, the test is running in a build
    // that makes use of the override pref. For testing purposes, unset the pref.
    Preferences.set(TelemetryUtils.Preferences.OverrideUpdateChannel, "");
  }

  // Check that we return the same channel as UpdateUtils, by default
  Assert.equal(TelemetryUtils.getUpdateChannel(), UpdateUtils.getUpdateChannel(false),
               "The telemetry reported channel must match the one from UpdateChannel, by default.");

  // Now set the override pref and check that we return the correct channel
  const OVERRIDE_TEST_CHANNEL = "nightly-test";
  Preferences.set(TelemetryUtils.Preferences.OverrideUpdateChannel, OVERRIDE_TEST_CHANNEL);
  Assert.equal(TelemetryUtils.getUpdateChannel(), OVERRIDE_TEST_CHANNEL,
               "The telemetry reported channel must match the override pref when pref is set.");
});
