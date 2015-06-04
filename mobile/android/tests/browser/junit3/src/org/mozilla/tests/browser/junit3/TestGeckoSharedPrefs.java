/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.tests.browser.junit3;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.preference.PreferenceManager;
import android.test.InstrumentationTestCase;
import android.test.RenamingDelegatingContext;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.GeckoSharedPrefs.Flags;

import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.Set;

/**
 * Test GeckoSharedPrefs migrations.
 */
public class TestGeckoSharedPrefs extends InstrumentationTestCase {

    private static class TestContext extends RenamingDelegatingContext {
        private static final String PREFIX = "TestGeckoSharedPrefs-";

        private final Set<String> usedPrefs;

        public TestContext(Context context) {
            super(context, PREFIX);
            usedPrefs = Collections.synchronizedSet(new HashSet<String>());
        }

        @Override
        public SharedPreferences getSharedPreferences(String name, int mode) {
            usedPrefs.add(name);
            return super.getSharedPreferences(PREFIX + name, mode);
        }

        public void clearUsedPrefs() {
            for (String prefsName : usedPrefs) {
                getSharedPreferences(prefsName, 0).edit().clear().commit();
            }

            usedPrefs.clear();
        }
    }

    private static final EnumSet<Flags> disableMigrations = EnumSet.of(Flags.DISABLE_MIGRATIONS);

    private TestContext context;

    protected void setUp() {
        context = new TestContext(getInstrumentation().getTargetContext());
    }

    protected void tearDown() {
        context.clearUsedPrefs();
        GeckoSharedPrefs.reset();
    }

    public void testDisableMigrations() {
        // Version is 0 before any migration
        assertEquals(0, GeckoSharedPrefs.getVersion(context));

        // Get prefs with migrations disabled
        GeckoSharedPrefs.forApp(context, disableMigrations);
        GeckoSharedPrefs.forProfile(context, disableMigrations);
        GeckoSharedPrefs.forProfileName(context, "someProfile", disableMigrations);

        // Version should still be 0
        assertEquals(0, GeckoSharedPrefs.getVersion(context));
    }

    public void testPrefsVersion() {
        // Version is 0 before any migration
        assertEquals(0, GeckoSharedPrefs.getVersion(context));

        // Trigger migration by getting a SharedPreferences instance
        GeckoSharedPrefs.forApp(context);

        // Version should be current after migration
        assertEquals(GeckoSharedPrefs.PREFS_VERSION, GeckoSharedPrefs.getVersion(context));
    }

    public void testMigrateFromPreferenceManager() {
        SharedPreferences appPrefs = GeckoSharedPrefs.forApp(context, disableMigrations);
        assertTrue(appPrefs.getAll().isEmpty());
        final Editor appEditor = appPrefs.edit();

        SharedPreferences profilePrefs = GeckoSharedPrefs.forProfileName(context, GeckoProfile.DEFAULT_PROFILE, disableMigrations);
        assertTrue(profilePrefs.getAll().isEmpty());
        final Editor profileEditor = profilePrefs.edit();

        final SharedPreferences pmPrefs = PreferenceManager.getDefaultSharedPreferences(context);
        assertTrue(pmPrefs.getAll().isEmpty());
        Editor pmEditor = pmPrefs.edit();

        // Insert a key for each type to exercise the
        // migration path a bit more thoroughly.
        pmEditor.putInt("int_key", 23);
        pmEditor.putLong("long_key", 23L);
        pmEditor.putString("string_key", "23");
        pmEditor.putFloat("float_key", 23.3f);

        final String[] profileKeys = {
            "string_profile",
            "int_profile"
        };

        // Insert keys that are expected to be moved to the
        // PROFILE scope.
        pmEditor.putString(profileKeys[0], "24");
        pmEditor.putInt(profileKeys[1], 24);

        // Commit changes to PreferenceManager
        pmEditor.commit();
        assertEquals(6, pmPrefs.getAll().size());

        // Perform actual migration with the given editors
        pmEditor = GeckoSharedPrefs.migrateFromPreferenceManager(context, appEditor,
                profileEditor, Arrays.asList(profileKeys));

        // Commit changes applied during the migration
        appEditor.commit();
        profileEditor.commit();
        pmEditor.commit();

        // PreferenceManager should have no keys
        assertTrue(pmPrefs.getAll().isEmpty());

        // App should have all keys except the profile ones
        assertEquals(4, appPrefs.getAll().size());

        // Ensure app scope doesn't have any of the profile keys
        for (int i = 0; i < profileKeys.length; i++) {
            assertFalse(appPrefs.contains(profileKeys[i]));
        }

        // Check app keys
        assertEquals(23, appPrefs.getInt("int_key", 0));
        assertEquals(23L, appPrefs.getLong("long_key", 0L));
        assertEquals("23", appPrefs.getString("string_key", ""));
        assertEquals(23.3f, appPrefs.getFloat("float_key", 0));

        assertEquals(2, profilePrefs.getAll().size());
        assertEquals("24", profilePrefs.getString(profileKeys[0], ""));
        assertEquals(24, profilePrefs.getInt(profileKeys[1], 0));
    }
}
