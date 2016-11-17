/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;


/**
 * Unit tests for float utilities.
 */
@RunWith(TestRunner.class)
public class TestFloatUtils {
    @Test
    public void testZeros() {
        assertTrue(FloatUtils.fuzzyEquals(0, 0));
    }

    @Test
    public void testNotEqual() {
        assertFalse(FloatUtils.fuzzyEquals(10, 0));
    }

    @Test
    public void testEqualsPromoted() {
        assertTrue(FloatUtils.fuzzyEquals(5, 5));
    }

    @Test
    public void testEqualsUnPromoted() {
        assertTrue(FloatUtils.fuzzyEquals(5.6f, 5.6f));
    }

    @Test
    public void testClampSuccess() {
        assertTrue(3 == FloatUtils.clamp(3, 1, 5));
    }

    @Test
    public void testClampFail() {
        try {
            FloatUtils.clamp(3, 5, 1);
        } catch (IllegalArgumentException e) {
            assertNotNull(e);
        }
    }

    @Test
    public void testInterpolateSuccess() {
        assertTrue(4 == FloatUtils.interpolate(1, 2, 3));
    }

    @Test
    public void testInterpolateFail() {
        assertFalse(3 == FloatUtils.interpolate(1, 2, 3));
    }


}
