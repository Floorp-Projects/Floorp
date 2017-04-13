/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.util;

import org.junit.Test;

import static org.junit.Assert.*;

public class StringUtilsTest {
    @Test
    public void testSafeSubstring() {
        assertEquals("hello", StringUtils.safeSubstring("hello", 0, 20));
        assertEquals("hell", StringUtils.safeSubstring("hello", 0, 4));
        assertEquals("hello", StringUtils.safeSubstring("hello", -5, 15));
    }
}