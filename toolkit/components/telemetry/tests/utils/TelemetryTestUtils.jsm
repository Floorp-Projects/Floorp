/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["TelemetryTestUtils"];

ChromeUtils.import("resource://testing-common/Assert.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

var TelemetryTestUtils = {
  /* Scalars */

  /**
   * An helper that asserts the value of a scalar if it's expected to be > 0,
   * otherwise makes sure that the scalar has not been reported.
   *
   * @param {Object} scalars The snapshot of the scalars.
   * @param {String} scalarName The name of the scalar to check.
   * @param {Number} value The expected value for the provided scalar.
   * @param {String} msg The message to print when checking the value.
   */
  assertScalar(scalars, scalarName, value, msg) {
    if (value > 0) {
      Assert.equal(scalars[scalarName], value, msg);
      return;
    }
    Assert.ok(!(scalarName in scalars), scalarName + " must not be reported.");
  },

  /**
   * Asserts if the snapshotted keyed scalars contain the expected
   * data.
   *
   * @param {Object} scalars The snapshot of the keyed scalars.
   * @param {String} scalarName The name of the keyed scalar to check.
   * @param {String} key The key that must be within the keyed scalar.
   * @param {String|Boolean|Number} expectedValue The expected value for the
   *        provided key in the scalar.
   */
  assertKeyedScalar(scalars, scalarName, key, expectedValue) {
    Assert.ok(scalarName in scalars,
              scalarName + " must be recorded.");
    Assert.ok(key in scalars[scalarName],
              scalarName + " must contain the '" + key + "' key.");
    Assert.equal(scalars[scalarName][key], expectedValue,
              scalarName + "['" + key + "'] must contain the expected value");
  },

  /**
   * Returns a snapshot of scalars from the parent-process.
   *
   * @param {Number} aChannel The channel dataset type from nsITelemetry.
   * @param {boolean} [aKeyed] Set to true if keyed scalars rather than normal
   *   scalars should be snapshotted.
   * @param {boolean} [aClear] Set to true to clear the scalars once the snapshot
   *   has been obtained.
   * @returns {Object} The snapshotted scalars from the parent process.
   */
  getParentProcessScalars(aChannel, aKeyed = false, aClear = false) {
    const extended = aChannel == Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN;
    const currentExtended = Services.telemetry.canRecordExtended;
    Services.telemetry.canRecordExtended = extended;
    const scalars = aKeyed ?
      Services.telemetry.getSnapshotForKeyedScalars("main", aClear).parent :
      Services.telemetry.getSnapshotForScalars("main", aClear).parent;
    Services.telemetry.canRecordExtended = currentExtended;
    return scalars || {};
  },

  /* Events */

  /**
   * Asserts if snapshotted events telemetry match the expected values.
   *
   * @param {Array} events Snapshotted telemetry events to test.
   * @param {Array} expectedEvents The expected event data.
   */
  assertEvents(events, expectedEvents) {
    if (!Services.telemetry.canRecordExtended) {
      console.log("Not asserting event telemetry - canRecordExtended is disabled.");
      return;
    }

    Assert.equal(events.length, expectedEvents.length, "Should have matching amount of events.");

    // Strip timestamps from the events for easier comparison.
    events = events.map(e => e.slice(1));

    for (let i = 0; i < events.length; ++i) {
      Assert.deepEqual(events[i], expectedEvents[i], "Events should match.");
    }
  },

  /* Histograms */

  /**
   * Clear and get the named histogram.
   *
   * @param {String} name The name of the histogram
   * @returns {Object} The obtained histogram.
   */
  getAndClearHistogram(name) {
    let histogram = Services.telemetry.getHistogramById(name);
    histogram.clear();
    return histogram;
  },


  /**
   * Clear and get the named keyed histogram.
   *
   * @param {String} name The name of the keyed histogram
   * @returns {Object} The obtained keyed histogram.
   */
  getAndClearKeyedHistogram(name) {
    let histogram = Services.telemetry.getKeyedHistogramById(name);
    histogram.clear();
    return histogram;
  },

  /**
   * Assert that the histogram index is the right value. It expects that
   * other indexes are all zero.
   *
   * @param {Object} histogram The histogram to check.
   * @param {Number} index The index to check against the expected value.
   * @param {Number} expected The expected value of the index.
   */
  assertHistogram(histogram, index, expected) {
    const snapshot = histogram.snapshot();
    for (let [i, val] of Object.entries(snapshot.values)) {
      if (i == index) {
        Assert.equal(val, expected,
          `expected counts should match for the histogram index ${i}`);
      } else {
        Assert.equal(val, 0,
          `unexpected counts should be zero for the histogram index ${i}`);
      }
    }
  },

  /**
   * Assert that a key within a keyed histogram contains the required sum.
   *
   * @param {Object} histogram The keyed histogram to check.
   * @param {String} key The key to check.
   * @param {Number} [expected] The expected sum for the key.
   */
  assertKeyedHistogramSum(histogram, key, expected) {
    const snapshot = histogram.snapshot();
    if (expected === undefined) {
      Assert.ok(!(key in snapshot), `The histogram must not contain ${key}.`);
      return;
    }
    Assert.ok(key in snapshot, `The histogram must contain ${key}.`);
    Assert.equal(snapshot[key].sum, expected,
      `The key ${key} must contain the expected sum.`);
  },

  /**
   * Assert that the value of a key within a keyed histogram is the right value.
   * It expects that other values are all zero.
   *
   * @param {Object} histogram The keyed histogram to check.
   * @param {String} key The key to check.
   * @param {Number} index The index to check against the expected value.
   * @param {Number} [expected] The expected values for the key.
   */
  assertKeyedHistogramValue(histogram, key, index, expected) {
    const snapshot = histogram.snapshot();
    for (let [i, val] of Object.entries(snapshot[key].values)) {
      if (i == index) {
        Assert.equal(val, expected,
          `expected counts should match for the histogram index ${i}`);
      } else {
        Assert.equal(val, 0,
          `unexpected counts should be zero for the histogram index ${i}`);
      }
    }
  },
};
