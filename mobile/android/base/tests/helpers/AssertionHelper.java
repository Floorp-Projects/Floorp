/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

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

    public static void assertEquals(final String message, final Object expected, final Object actual) {
        sAsserter.is(actual, expected, message);
    }

    public static void assertEquals(final String message, final int expected, final int actual) {
        sAsserter.is(actual, expected, message);
    }

    public static void assertFalse(final String message, final boolean actual) {
        sAsserter.ok(!actual, message, DIAG_STRING);
    }

    public static void assertNotNull(final String message, final Object actual) {
        sAsserter.isnot(actual, null, message);
    }

    public static void assertNull(final String message, final Object actual) {
        sAsserter.is(actual, null, message);
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
