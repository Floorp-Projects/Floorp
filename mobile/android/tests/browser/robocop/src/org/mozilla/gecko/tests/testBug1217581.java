/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;


import org.mozilla.gecko.Telemetry;

public class testBug1217581 extends BaseTest {
    // Take arbitrary histogram names used by Fennec.
    private static final String TEST_HISTOGRAM_NAME = "FENNEC_SYNC_NUMBER_OF_SYNCS_COMPLETED";
    private static final String TEST_KEYED_HISTOGRAM_NAME = "FX_MIGRATION_ERRORS";
    private static final String TEST_KEY_NAME = "testBug1217581";


    public void testBug1217581() {
        blockForGeckoReady();

        mAsserter.ok(true, "Checking that adding to a keyed histogram then adding to a normal histogram does not cause a crash.", "");
        Telemetry.addToKeyedHistogram(TEST_KEYED_HISTOGRAM_NAME, TEST_KEY_NAME, 1);
        Telemetry.addToHistogram(TEST_HISTOGRAM_NAME, 1);
        mAsserter.ok(true, "Adding to a keyed histogram then to a normal histogram was a success!", "");

        mAsserter.ok(true, "Checking that adding to a normal histogram then adding to a keyed histogram does not cause a crash.", "");
        Telemetry.addToHistogram(TEST_HISTOGRAM_NAME, 1);
        Telemetry.addToKeyedHistogram(TEST_KEYED_HISTOGRAM_NAME, TEST_KEY_NAME, 1);
        mAsserter.ok(true, "Adding to a normal histogram then to a keyed histogram was a success!", "");
    }
}
