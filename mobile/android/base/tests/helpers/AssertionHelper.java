/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import java.util.Arrays;

import org.mozilla.gecko.Assert;
import org.mozilla.gecko.tests.UITestContext;

/**
 * Provides assertions in a JUnit-like API that wraps the robocop Assert interface.
 */
public final class AssertionHelper {
    // Assert.ok has a "diag" ("diagnostic") parameter that has no useful purpose.
    private static final String DIAG_STRING = "";

    private static Assert sAsserter;

    private AssertionHelper() { /* To disallow instantation. */ }

    protected static void init(final UITestContext context) {
        sAsserter = context.getAsserter();
    }

    public static void assertArrayEquals(final String message, final byte[] expecteds, final byte[] actuals) {
        sAsserter.ok(Arrays.equals(expecteds, actuals), message, DIAG_STRING);
    }

    public static void assertArrayEquals(final String message, final char[] expecteds, final char[] actuals) {
        sAsserter.ok(Arrays.equals(expecteds, actuals), message, DIAG_STRING);
    }

    public static void assertArrayEquals(final String message, final short[] expecteds, final short[] actuals) {
        sAsserter.ok(Arrays.equals(expecteds, actuals), message, DIAG_STRING);
    }

    public static void assertArrayEquals(final String message, final int[] expecteds, final int[] actuals) {
        sAsserter.ok(Arrays.equals(expecteds, actuals), message, DIAG_STRING);
    }

    public static void assertArrayEquals(final String message, final long[] expecteds, final long[] actuals) {
        sAsserter.ok(Arrays.equals(expecteds, actuals), message, DIAG_STRING);
    }

    public static void assertArrayEquals(final String message, final Object[] expecteds, final Object[] actuals) {
        sAsserter.ok(Arrays.equals(expecteds, actuals), message, DIAG_STRING);
    }

    public static void assertEquals(final String message, final double expected, final double actual, final double delta) {
        if (Double.compare(expected, actual) != 0) {
            sAsserter.ok(Math.abs(expected - actual) <= delta, message, DIAG_STRING);
        }
    }

    public static void assertEquals(final String message, final long expected, final long actual) {
        sAsserter.is(actual, expected, message);
    }

    public static void assertEquals(final String message, final Object expected, final Object actual) {
        sAsserter.is(actual, expected, message);
    }

    public static void assertFalse(final String message, final boolean actual) {
        sAsserter.ok(!actual, message, DIAG_STRING);
    }

    public static void assertNotNull(final String message, final Object actual) {
        sAsserter.isnot(actual, null, message);
    }

    public static void assertNotSame(final String message, final Object unexpected, final Object actual) {
        sAsserter.ok(unexpected != actual, message, DIAG_STRING);
    }

    public static void assertNull(final String message, final Object actual) {
        sAsserter.is(actual, null, message);
    }

    public static void assertSame(final String message, final Object expected, final Object actual) {
        sAsserter.ok(expected == actual, message, DIAG_STRING);
    }

    public static void assertTrue(final String message, final boolean actual) {
        sAsserter.ok(actual, message, DIAG_STRING);
    }

    public static void assertIsPixel(final String message, final int actual, final int r, final int g, final int b) {
	sAsserter.ispixel(actual, r, g, b, message);
    }

    public static void assertIsNotPixel(final String message, final int actual, final int r, final int g, final int b) {
	sAsserter.isnotpixel(actual, r, g, b, message);
    }

    public static void fail(final String message) {
        sAsserter.ok(false, message, DIAG_STRING);
    }
}
