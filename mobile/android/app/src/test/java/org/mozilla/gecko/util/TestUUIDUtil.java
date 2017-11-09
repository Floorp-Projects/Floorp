/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.*;

/**
 * Tests for uuid utils.
 */
@RunWith(TestRunner.class)
public class TestUUIDUtil {
    private static final String[] validUUIDs = {
        "904cd9f8-af63-4525-8ce0-b9127e5364fa",
        "8d584bd2-00ea-4043-a617-ed4ce7018ed0",
        "3abad327-2669-4f68-b9ef-7ace8c5314d6",
    };

    private static final String[] invalidUUIDs = {
        "its-not-a-uuid-mate",
        "904cd9f8-af63-4525-8ce0-b9127e5364falol",
        "904cd9f8-af63-4525-8ce0-b9127e5364f",
    };

    @Test
    public void testUUIDRegex() {
        for (final String uuid : validUUIDs) {
            assertTrue("Valid UUID matches UUID-regex", uuid.matches(UUIDUtil.UUID_REGEX));
        }
        for (final String uuid : invalidUUIDs) {
            assertFalse("Invalid UUID does not match UUID-regex", uuid.matches(UUIDUtil.UUID_REGEX));
        }
    }

    @Test
    public void testUUIDPattern() {
        for (final String uuid : validUUIDs) {
            assertTrue("Valid UUID matches UUID-regex", UUIDUtil.UUID_PATTERN.matcher(uuid).matches());
        }
        for (final String uuid : invalidUUIDs) {
            assertFalse("Invalid UUID does not match UUID-regex", UUIDUtil.UUID_PATTERN.matcher(uuid).matches());
        }
    }
}
