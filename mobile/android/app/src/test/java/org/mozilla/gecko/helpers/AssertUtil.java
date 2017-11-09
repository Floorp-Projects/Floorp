/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.helpers;

import org.junit.Assert;

/**
 * Some additional assert methods on top of org.junit.Assert.
 */
public class AssertUtil {
    /**
     * Asserts that the String {@code text} contains the String {@code sequence}. If it doesn't then
     * an {@link AssertionError} will be thrown.
     */
    public static void assertContains(String text, String sequence) {
        Assert.assertTrue(text.contains(sequence));
    }

    /**
     * Asserts that the String {@code text} contains not the String {@code sequence}. If it does
     * then an {@link AssertionError} will be thrown.
     */
    public static void assertContainsNot(String text, String sequence) {
        Assert.assertFalse(text.contains(sequence));
    }
}
