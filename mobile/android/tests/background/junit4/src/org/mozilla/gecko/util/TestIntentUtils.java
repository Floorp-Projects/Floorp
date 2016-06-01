/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.content.Intent;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import static org.junit.Assert.*;

/**
 * Tests for the Intent utilities.
 */
@RunWith(TestRunner.class)
public class TestIntentUtils {

    private static final Map<String, String> TEST_ENV_VAR_MAP;
    static {
        final HashMap<String, String> tempMap = new HashMap<>();
        tempMap.put("ZERO", "0");
        tempMap.put("ONE", "1");
        tempMap.put("STRING", "TEXT");
        tempMap.put("L_WHITESPACE", " LEFT");
        tempMap.put("R_WHITESPACE", "RIGHT ");
        tempMap.put("ALL_WHITESPACE", " ALL ");
        tempMap.put("WHITESPACE_IN_VALUE", "IN THE MIDDLE");
        tempMap.put("WHITESPACE IN KEY", "IS_PROBABLY_NOT_VALID_ANYWAY");
        tempMap.put("BLANK_VAL", "");
        TEST_ENV_VAR_MAP = Collections.unmodifiableMap(tempMap);
    }

    private Intent testIntent;

    @Before
    public void setUp() throws Exception {
        testIntent = getIntentWithTestData();
    }

    private static Intent getIntentWithTestData() {
        final Intent out = new Intent(Intent.ACTION_VIEW);
        int i = 0;
        for (final String key : TEST_ENV_VAR_MAP.keySet()) {
            final String value = key + "=" + TEST_ENV_VAR_MAP.get(key);
            out.putExtra("env" + i, value);
            i += 1;
        }
        return out;
   }

    @Test
    public void testGetEnvVarMap() throws Exception {
        final HashMap<String, String> actual = IntentUtils.getEnvVarMap(testIntent);
        for (final String actualEnvVarName : actual.keySet()) {
            assertTrue("Actual key exists in test data: " + actualEnvVarName,
                    TEST_ENV_VAR_MAP.containsKey(actualEnvVarName));

            final String expectedValue = TEST_ENV_VAR_MAP.get(actualEnvVarName);
            final String actualValue = actual.get(actualEnvVarName);
            assertEquals("Actual env var value matches test data", expectedValue, actualValue);
        }
    }
}