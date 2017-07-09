/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;


/**
 * Unit tests for float utilities.
 */
@RunWith(TestRunner.class)
public class TestFloatUtils {

    @Test
    public void testEqualIfComparingZeros() {
        assertTrue(FloatUtils.fuzzyEquals(0, 0));
    }

    @Test
    public void testEqualFailIf5thDigitIsDifferent() {
        assertFalse(FloatUtils.fuzzyEquals(0.00001f, 0.00002f));
    }

    @Test
    public void testEqualSuccessIf6thDigitIsDifferent() {
        assertTrue(FloatUtils.fuzzyEquals(0.000001f, 0.000002f));
    }

    @Test
    public void testEqualFail() {
        assertFalse(FloatUtils.fuzzyEquals(10, 0));
    }

    @Test
    public void testEqualSuccessIfPromoted() {
        assertTrue(FloatUtils.fuzzyEquals(5, 5));
    }

    @Test
    public void testEqualSuccessIfUnPromoted() {
        assertTrue(FloatUtils.fuzzyEquals(5.6f, 5.6f));
    }
}
