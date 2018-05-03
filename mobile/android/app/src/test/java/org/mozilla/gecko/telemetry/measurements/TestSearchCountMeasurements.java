/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.telemetry.measurements;

import android.content.Context;
import android.content.SharedPreferences;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

import static org.junit.Assert.*;

/**
 * Tests for the class that stores search count measurements.
 */
@RunWith(RobolectricTestRunner.class)
public class TestSearchCountMeasurements {

    private SharedPreferences sharedPrefs;

    @Before
    public void setUp() throws Exception {
        sharedPrefs = RuntimeEnvironment.application.getSharedPreferences(
                TestSearchCountMeasurements.class.getSimpleName(), Context.MODE_PRIVATE);
    }

    private void assertNewValueInsertedNoIncrementedValues(final int expectedKeyCount) {
        assertEquals("Shared prefs key count has incremented", expectedKeyCount, sharedPrefs.getAll().size());
        assertTrue("Shared prefs still contains non-incremented initial value", sharedPrefs.getAll().containsValue(1));
        assertFalse("Shared prefs has not incremented any values", sharedPrefs.getAll().containsValue(2));
    }

    @Test
    public void testIncrementSearchCanRecreateEngineAndWhere() throws Exception {
        final String expectedIdentifier = "google";
        final String expectedWhere = "suggestbar";

        SearchCountMeasurements.incrementSearch(sharedPrefs, expectedIdentifier, expectedWhere);
        assertFalse("Shared prefs has some values", sharedPrefs.getAll().isEmpty());
        assertTrue("Shared prefs contains initial value", sharedPrefs.getAll().containsValue(1));

        boolean foundEngine = false;
        for (final String key : sharedPrefs.getAll().keySet()) {
            // We could try to match the exact key, but that's more fragile.
            if (key.contains(expectedIdentifier) && key.contains(expectedWhere)) {
                foundEngine = true;
            }
        }
        assertTrue("SharedPrefs keyset contains enough info to recreate engine & where", foundEngine);
    }

    @Test
    public void testIncrementSearchCalledMultipleTimesSameEngine() throws Exception {
        final String engineIdentifier = "whatever";
        final String where = "wherever";

        SearchCountMeasurements.incrementSearch(sharedPrefs, engineIdentifier, where);
        assertFalse("Shared prefs has some values", sharedPrefs.getAll().isEmpty());
        assertTrue("Shared prefs contains initial value", sharedPrefs.getAll().containsValue(1));

        // The initial key count storage saves metadata so we can't verify the number of keys is only 1. However,
        // we assume subsequent calls won't add additional metadata and use it to verify the key count.
        final int keyCountAfterFirst = sharedPrefs.getAll().size();
        for (int i = 2; i <= 3; ++i) {
            SearchCountMeasurements.incrementSearch(sharedPrefs, engineIdentifier, where);
            assertEquals("Shared prefs key count has not changed", keyCountAfterFirst, sharedPrefs.getAll().size());
            assertTrue("Shared prefs incremented", sharedPrefs.getAll().containsValue(i));
        }
    }

    @Test
    public void testIncrementSearchCalledMultipleTimesSameEngineDifferentWhere() throws Exception {
        final String engineIdenfitier = "whatever";

        SearchCountMeasurements.incrementSearch(sharedPrefs, engineIdenfitier, "one place");
        assertFalse("Shared prefs has some values", sharedPrefs.getAll().isEmpty());
        assertTrue("Shared prefs contains initial value", sharedPrefs.getAll().containsValue(1));

        // The initial key count storage saves metadata so we can't verify the number of keys is only 1. However,
        // we assume subsequent calls won't add additional metadata and use it to verify the key count.
        final int keyCountAfterFirst = sharedPrefs.getAll().size();
        for (int i = 1; i <= 2; ++i) {
            SearchCountMeasurements.incrementSearch(sharedPrefs, engineIdenfitier, "another place " + i);
            assertNewValueInsertedNoIncrementedValues(keyCountAfterFirst + i);
        }
    }

    @Test
    public void testIncrementSearchCalledMultipleTimesDifferentEngines() throws Exception {
        final String where = "wherever";

        SearchCountMeasurements.incrementSearch(sharedPrefs, "steam engine", where);
        assertFalse("Shared prefs has some values", sharedPrefs.getAll().isEmpty());
        assertTrue("Shared prefs contains initial value", sharedPrefs.getAll().containsValue(1));

        // The initial key count storage saves metadata so we can't verify the number of keys is only 1. However,
        // we assume subsequent calls won't add additional metadata and use it to verify the key count.
        final int keyCountAfterFirst = sharedPrefs.getAll().size();
        for (int i = 1; i <= 2; ++i) {
            SearchCountMeasurements.incrementSearch(sharedPrefs, "combustion engine" + i, where);
            assertNewValueInsertedNoIncrementedValues(keyCountAfterFirst + i);
        }
    }

    @Test // assumes the format saved in SharedPrefs to store test data
    public void testGetAndZeroSearchDeletesPrefs() throws Exception {
        assertTrue("Shared prefs is empty", sharedPrefs.getAll().isEmpty());

        final SharedPreferences.Editor editor = sharedPrefs.edit();
        final Set<String> engineKeys = new HashSet<>(Arrays.asList("whatever.yeah", "lol.what"));
        editor.putStringSet(SearchCountMeasurements.PREF_SEARCH_KEYSET, engineKeys);
        for (final String key : engineKeys) {
            editor.putInt(getEngineSearchCountKey(key), 1);
        }
        editor.apply();
        assertFalse("Shared prefs is not empty after test data inserted", sharedPrefs.getAll().isEmpty());

        SearchCountMeasurements.getAndZeroSearch(sharedPrefs);
        assertTrue("Shared prefs is empty after zero", sharedPrefs.getAll().isEmpty());
    }

    @Test // assumes the format saved in SharedPrefs to store test data
    public void testGetAndZeroSearchVerifyReturnedData() throws Exception {
        final HashMap<String, Integer> expected = new HashMap<>();
        expected.put("steamengine.here", 1337);
        expected.put("combustionengine.there", 10);

        final SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(SearchCountMeasurements.PREF_SEARCH_KEYSET, expected.keySet());
        for (final String key : expected.keySet()) {
            editor.putInt(SearchCountMeasurements.getEngineSearchCountKey(key), expected.get(key));
        }
        editor.apply();
        assertFalse("Shared prefs is not empty after test data inserted", sharedPrefs.getAll().isEmpty());

        final ExtendedJSONObject actual = SearchCountMeasurements.getAndZeroSearch(sharedPrefs);
        assertEquals("Returned JSON contains number of items inserted", expected.size(), actual.size());
        for (final String key : expected.keySet()) {
            assertEquals("Returned JSON contains inserted value", expected.get(key), actual.getIntegerSafely(key));
        }
    }

    @Test
    public void testGetAndZeroSearchNoData() throws Exception {
        final ExtendedJSONObject actual = SearchCountMeasurements.getAndZeroSearch(sharedPrefs);
        assertEquals("Returned json is empty", 0, actual.size());
    }

    private String getEngineSearchCountKey(final String engineWhereStr) {
        return SearchCountMeasurements.getEngineSearchCountKey(engineWhereStr);
    }
}
