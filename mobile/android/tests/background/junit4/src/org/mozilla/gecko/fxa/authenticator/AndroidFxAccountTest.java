/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.fxa.authenticator;

import android.content.Context;
import android.content.SharedPreferences;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.HashSet;
import java.util.Set;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class AndroidFxAccountTest {
    @Test
    public void testMigrateSharedPreferences() throws Exception {
        final SharedPreferences preferences = RuntimeEnvironment.application.getSharedPreferences("new_prefs", Context.MODE_PRIVATE);
        final SharedPreferences oldPreferences = RuntimeEnvironment.application.getSharedPreferences("old_prefs", Context.MODE_PRIVATE);

        // Test trivial case: nothing to migrate.
        SharedPreferences newPrefs = AndroidFxAccount.doMaybeMigrateSharedPreferences(preferences, oldPreferences);
        assertEquals(0, newPrefs.getAll().size());

        // Test trivial case: migration already happened at some point. Ensure nothing is copied.
        oldPreferences.edit().putString("evil", "string").apply();
        preferences.edit().putString("migrated", "yup").apply();
        newPrefs = AndroidFxAccount.doMaybeMigrateSharedPreferences(preferences, oldPreferences);
        assertEquals(1, newPrefs.getAll().size());
        assertEquals("yup", preferences.getString("migrated", null));
        assertFalse(preferences.contains("evil"));

        // Prepare data for a regular test.
        final Set<String> testStringSet = new HashSet<>();
        testStringSet.add("life");
        testStringSet.add("universe");
        testStringSet.add("everything");

        oldPreferences.edit()
                .putBoolean("testBoolean", true)
                .putInt("testInt", 42)
                .putString("testString", "mostly harmless")
                .putStringSet("testStringSet", testStringSet)
                .putLong("testLong", 1985L)
                .putFloat("testFloat", 1982F)
                .apply();

        // Pretend migration never happened.
        preferences.edit().clear().apply();

        // Test migration with every allowable data type present.
        newPrefs = AndroidFxAccount.doMaybeMigrateSharedPreferences(preferences, oldPreferences);

        // Test that values have been carried over.
        assertEquals(true, newPrefs.getBoolean("testBoolean", false));
        assertEquals(42, newPrefs.getInt("testInt", 0));
        assertEquals("mostly harmless", newPrefs.getString("testString", null));

        final Set<String> copiedStringSet = newPrefs.getStringSet("testStringSet", null);
        assertNotNull(copiedStringSet);
        assertEquals(3, copiedStringSet.size());
        assertTrue(copiedStringSet.contains("life"));
        assertTrue(copiedStringSet.contains("universe"));
        assertTrue(copiedStringSet.contains("everything"));

        assertEquals(1985L, newPrefs.getLong("testLong", 0));
        assertEquals(1982F, newPrefs.getFloat("testFloat", 0), 0);

        // Test that old prefs have been cleared.
        assertEquals(0, oldPreferences.getAll().size());
    }
}
