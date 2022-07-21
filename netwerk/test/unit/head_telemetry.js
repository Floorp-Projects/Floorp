/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

var HandshakeTelemetryHelpers = {
  HISTOGRAMS: ["SSL_HANDSHAKE_RESULT", "SSL_TIME_UNTIL_READY"],
  FLAVORS: ["", "_FIRST_TRY", "_CONSERVATIVE", "_ECH", "_ECH_GREASE"],

  /**
   * Prints the Histogram to console.
   *
   * @param {*} name The identifier of the Histogram.
   */
  dumpHistogram(name) {
    let values = Services.telemetry.getHistogramById(name).snapshot().values;
    dump(`${name}: ${JSON.stringify(values)}\n`);
  },

  /**
   * Counts the number of entries in the histogram, ignoring the bucket value.
   * e.g. {0: 1, 1: 2, 3: 3} has 6 entries.
   *
   * @param {Object} histObject The histogram to count the entries of.
   * @returns The count of the number of entries in the histogram.
   */
  countHistogramEntries(histObject) {
    Assert.ok(
      !mozinfo.socketprocess_networking,
      "Histograms don't populate on network process"
    );
    let count = 0;
    let m = histObject.snapshot().values;
    for (let k in m) {
      count += m[k];
    }
    return count;
  },

  /**
   * Assert that the histogram index is the right value. It expects that
   * other indexes are all zero.
   *
   * @param {Object} histogram The histogram to check.
   * @param {Number} index The index to check against the expected value.
   * @param {Number} expected The expected value of the index.
   */
  assertHistogramMap(histogram, expectedEntries) {
    Assert.ok(
      !mozinfo.socketprocess_networking,
      "Histograms don't populate on network process"
    );
    let snapshot = JSON.parse(JSON.stringify(histogram.snapshot()));
    for (let [Tk, Tv] of expectedEntries.entries()) {
      let found = false;
      for (let [i, val] of Object.entries(snapshot.values)) {
        if (i == Tk) {
          found = true;
          Assert.equal(
            val,
            Tv,
            `expected counts should match for ${histogram.name()} at index ${i}`
          );
          snapshot.values[i] = 0; // Reset the value
        }
      }
      Assert.ok(
        found,
        `Should have found an entry for ${histogram.name()} at index ${Tk}`
      );
    }
    for (let k in snapshot.values) {
      Assert.equal(
        snapshot.values[k],
        0,
        `Should NOT have found an entry for ${histogram.name()} at index ${k} of value ${
          snapshot.values[k]
        }`
      );
    }
  },

  /**
   * Generates the pairwise concatonation of histograms and flavors.
   *
   * @param {Array} histogramList A subset of HISTOGRAMS.
   * @param {Array} flavorList  A subset of FLAVORS.
   * @returns {Array} Valid TLS Histogram identifiers
   */
  getHistogramNames(histogramList, flavorList) {
    let output = [];
    for (let h of histogramList) {
      Assert.ok(this.HISTOGRAMS.includes(h), "Histogram name valid");
      for (let f of flavorList) {
        Assert.ok(this.FLAVORS.includes(f), "Histogram flavor valid");
        output.push(h.concat(f));
      }
    }
    return output;
  },

  /**
   * getHistogramNames but mapped to Histogram objects.
   */
  getHistograms(histogramList, flavorList) {
    return this.getHistogramNames(histogramList, flavorList).map(x =>
      Services.telemetry.getHistogramById(x)
    );
  },

  /**
   * Clears TLS Handshake Histograms.
   */
  resetHistograms() {
    let allHistograms = this.getHistograms(this.HISTOGRAMS, this.FLAVORS);
    for (let h of allHistograms) {
      h.clear();
    }
  },

  /**
   * Checks that all TLS Handshake Histograms of a particular flavor have
   * exactly resultCount entries for the resultCode and no other entries.
   *
   * @param {Array} flavors An array of strings corresponding to which types
   *                        of histograms should have entries. See
   *                        HandshakeTelemetryHelpers.FLAVORS.
   * @param {number} resultCode The expected result code, see sslerr.h. 0 is success, all others are errors.
   * @param {number} resultCount The number of handshake results expected.
   */
  checkEntry(flavors, resultCode, resultCount) {
    Assert.ok(
      !mozinfo.socketprocess_networking,
      "Histograms don't populate on network process"
    );
    // SSL_HANDSHAKE_RESULT_{FLAVOR}
    for (let h of this.getHistograms(["SSL_HANDSHAKE_RESULT"], flavors)) {
      TelemetryTestUtils.assertHistogram(h, resultCode, resultCount);
    }

    // SSL_TIME_UNTIL_READY_{FLAVOR} should only contain values if we expected success.
    if (resultCode === 0) {
      for (let h of this.getHistograms(["SSL_TIME_UNTIL_READY"], flavors)) {
        Assert.ok(
          this.countHistogramEntries(h) === resultCount,
          "Timing entry count correct"
        );
      }
    } else {
      for (let h of this.getHistograms(["SSL_TIME_UNTIL_READY"], flavors)) {
        Assert.ok(
          this.countHistogramEntries(h) === 0,
          "No timing entries expected"
        );
      }
    }
  },

  checkSuccess(flavors, resultCount = 1) {
    this.checkEntry(flavors, 0, resultCount);
  },

  checkEmpty(flavors) {
    for (let h of this.getHistogramNames(this.HISTOGRAMS, flavors)) {
      let hObj = Services.telemetry.getHistogramById(h);
      Assert.ok(
        this.countHistogramEntries(hObj) === 0,
        `No entries expected in ${h.name}. Contents: ${JSON.stringify(
          hObj.snapshot()
        )}`
      );
    }
  },
};
