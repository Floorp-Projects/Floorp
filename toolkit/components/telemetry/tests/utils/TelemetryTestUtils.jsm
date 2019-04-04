/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["TelemetryTestUtils"];

const {Assert} = ChromeUtils.import("resource://testing-common/Assert.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

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
   * Returns a snapshot of scalars from the specified process.
   *
   * @param {String} aProcessName Name of the process. Could be parent or
   *   something else.
   * @param {boolean} [aKeyed] Set to true if keyed scalars rather than normal
   *   scalars should be snapshotted.
   * @param {boolean} [aClear] Set to true to clear the scalars once the snapshot
   *   has been obtained.
   * @param {Number} aChannel The channel dataset type from nsITelemetry.
   * @returns {Object} The snapshotted scalars from the parent process.
   */
  getProcessScalars(aProcessName, aKeyed = false, aClear = false,
      aChannel = Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS) {
    const extended = aChannel == Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS;
    const currentExtended = Services.telemetry.canRecordExtended;
    Services.telemetry.canRecordExtended = extended;
    const scalars = aKeyed ?
      Services.telemetry.getSnapshotForKeyedScalars("main", aClear)[aProcessName] :
      Services.telemetry.getSnapshotForScalars("main", aClear)[aProcessName];
    Services.telemetry.canRecordExtended = currentExtended;
    return scalars || {};
  },

  /* Events */

  /**
   * Asserts that the number of events, after filtering, is equal to numEvents.
   *
   * @param {Number} numEvents The number of events to assert.
   * @param {Object} filter As per assertEvents.
   * @param {Object} options As per assertEvents.
   */
  assertNumberOfEvents(numEvents, filter, options) {
    // Create an array of empty objects of length numEvents
    TelemetryTestUtils.assertEvents(
      Array.from({length: numEvents}, () => ({})), filter, options);
  },

  /**
   * Asserts that, after optional filtering, the current events snapshot
   * matches expectedEvents.
   *
   * @param {Array} expectedEvents An array of event structures of the form
   *                [category, method, object, value, extra]
   *                or the same as an object with fields named as above.
   *                The array can be empty to assert that there are no events
   *                that match the filter.
   *                Each field can be absent/undefined (to match
   *                everything), a string or null (to match that value), a
   *                RegExp to match what it can match, or a function which
   *                matches by returning true when called with the field.
   *                `extra` is slightly different. If present it must be an
   *                object whose fields are treated the same way as the others.
   * @param {Object} filter An object of strings or RegExps for first filtering
   *                 the event snapshot. Of the form {category, method, object}.
   *                 Absent filters filter nothing.
   * @param {Object} options An object containing any of
   *                     - clear {bool} clear events. Default true.
   *                     - process {string} the process to examine. Default parent.
   */
  assertEvents(expectedEvents, filter = {}, {clear = true, process = "parent"} = {}) {
    // Step 0: Snapshot and clear.
    let snapshots = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, clear);
    if (expectedEvents.length === 0 && !(process in snapshots)) {
      // Job's done!
      return;
    }
    Assert.ok(process in snapshots, `${process} must be in snapshot. Has [${Object.keys(snapshots)}].`);
    let snapshot = snapshots[process];

    // Step 1: Filter.
    let {category: filterCategory, method: filterMethod, object: filterObject} = filter;
    let matches = (expected, actual) => {
      if (expected === undefined) {
        return true;
      } else if (expected instanceof RegExp) {
        return expected.test(actual);
      } else if ((typeof expected) === "function") {
        return expected(actual);
      }
      return expected === actual;
    };

    let filtered = snapshot
      .map(([/* timestamp */, category, method, object, value, extra]) => {
        // We don't care about the `timestamp` value.
        // Tests that examine that value should use `snapshotEvents` directly.
        return [category, method, object, value, extra];
      }).filter(([category, method, object]) => {
        return matches(filterCategory, category)
          && matches(filterMethod, method)
          && matches(filterObject, object);
      });

    // Step 2: Match.
    Assert.equal(expectedEvents.length, filtered.length,
      "After filtering we must have the expected number of events.");
    if (expectedEvents.length === 0) {
      // Job's done!
      return;
    }

    // Transform object-type expected events to array-type to match snapshot.
    if (!Array.isArray(expectedEvents[0])) {
      expectedEvents = expectedEvents.map(
        ({category, method, object, value, extra}) =>
          [category, method, object, value, extra]);
    }

    const FIELD_NAMES = ["category", "method", "object", "value", "extra"];
    const EXTRA_INDEX = 4;
    for (let i = 0; i < expectedEvents.length; ++i) {
      let expected = expectedEvents[i];
      let actual = filtered[i];

      // Match everything up to `extra`
      for (let j = 0; j < EXTRA_INDEX; ++j) {
        if (expected[j] === undefined) {
          // Don't spam the assert log with unspecified fields.
          continue;
        }
        Assert.report(!matches(expected[j], actual[j]), actual[j], expected[j],
            `${FIELD_NAMES[j]} in event ${actual[0]}#${actual[1]}#${actual[2]} must match.`,
            "matches");
      }

      // Match extra
      if (expected.length > EXTRA_INDEX && expected[EXTRA_INDEX] !== undefined) {
        Assert.ok(actual.length > EXTRA_INDEX,
            `Actual event ${actual[0]}#${actual[1]}#${actual[2]} expected to have extra.`);
        let expectedExtra = expected[EXTRA_INDEX];
        let actualExtra = actual[EXTRA_INDEX];
        for (let [key, value] of Object.entries(expectedExtra)) {
          Assert.ok(key in actualExtra,
              `Expected key ${key} must be in actual extra. Actual keys: [${Object.keys(actualExtra)}].`);
          Assert.report(!matches(value, actualExtra[key]), actualExtra[key], value,
              `extra[${key}] must match in event ${actual[0]}#${actual[1]}#${actual[2]}.`,
              "matches");
        }
      }
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
    let found = false;
    for (let [i, val] of Object.entries(snapshot.values)) {
      if (i == index) {
        found = true;
        Assert.equal(val, expected,
          `expected counts should match for ${histogram.name()} at index ${i}`);
      } else {
        Assert.equal(val, 0,
          `unexpected counts should be zero for ${histogram.name()} at index ${i}`);
      }
    }
    Assert.ok(found, `Should have found an entry for ${histogram.name()} at index ${index}`);
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
      Assert.ok(!(key in snapshot), `The histogram ${histogram.name()} must not contain ${key}.`);
      return;
    }
    Assert.ok(key in snapshot, `The histogram ${histogram.name()} must contain ${key}.`);
    Assert.equal(snapshot[key].sum, expected,
      `The key ${key} must contain the expected sum in ${histogram.name()}.`);
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
    if (!(key in snapshot)) {
      Assert.ok(false, `The histogram ${histogram.name()} must contain ${key}`);
      return;
    }
    for (let [i, val] of Object.entries(snapshot[key].values)) {
      if (i == index) {
        Assert.equal(val, expected,
          `expected counts should match for ${histogram.name()} at index ${i}`);
      } else {
        Assert.equal(val, 0,
          `unexpected counts should be zero for ${histogram.name() } at index ${i}`);
      }
    }
  },
};
