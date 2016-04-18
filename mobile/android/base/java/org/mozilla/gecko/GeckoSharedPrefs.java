/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.Arrays;
import java.util.EnumSet;
import java.util.List;
import java.util.Map;

import org.mozilla.gecko.annotation.RobocopTarget;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.os.StrictMode;
import android.preference.PreferenceManager;
import android.util.Log;

/**
 * {@code GeckoSharedPrefs} provides scoped SharedPreferences instances.
 * You should use this API instead of using Context.getSharedPreferences()
 * directly. There are four methods to get scoped SharedPreferences instances:
 *
 * forApp()
 *     Use it for app-wide, cross-profile pref keys.
 * forCrashReporter()
 *     For the crash reporter, which runs in its own process.
 * forProfile()
 *     Use it to fetch and store keys for the current profile.
 * forProfileName()
 *     Use it to fetch and store keys from/for a specific profile.
 *
 * {@code GeckoSharedPrefs} has a notion of migrations. Migrations can used to
 * migrate keys from one scope to another. You can trigger a new migration by
 * incrementing PREFS_VERSION and updating migrateIfNecessary() accordingly.
 *
 * Migration history:
 *     1: Move all PreferenceManager keys to app/profile scopes
 *     2: Move the crash reporter's private preferences into their own scope
 */
@RobocopTarget
public final class GeckoSharedPrefs {
    private static final String LOGTAG = "GeckoSharedPrefs";

    // Increment it to trigger a new migration
    public static final int PREFS_VERSION = 2;

    // Name for app-scoped prefs
    public static final String APP_PREFS_NAME = "GeckoApp";

    // Name for crash reporter prefs
    public static final String CRASH_PREFS_NAME = "CrashReporter";

    // Used when fetching profile-scoped prefs.
    public static final String PROFILE_PREFS_NAME_PREFIX = "GeckoProfile-";

    // The prefs key that holds the current migration
    private static final String PREFS_VERSION_KEY = "gecko_shared_prefs_migration";

    // For disabling migration when getting a SharedPreferences instance
    private static final EnumSet<Flags> disableMigrations = EnumSet.of(Flags.DISABLE_MIGRATIONS);

    // The keys that have to be moved from ProfileManager's default
    // shared prefs to the profile from version 0 to 1.
    private static final String[] PROFILE_MIGRATIONS_0_TO_1 = {
        "home_panels",
        "home_locale"
    };

    // The keys that have to be moved from the app prefs
    // into the crash reporter's own prefs.
    private static final String[] PROFILE_MIGRATIONS_1_TO_2 = {
        "sendReport",
        "includeUrl",
        "allowContact",
        "contactEmail"
    };

    // For optimizing the migration check in subsequent get() calls
    private static volatile boolean migrationDone;

    public enum Flags {
        DISABLE_MIGRATIONS
    }

    public static SharedPreferences forApp(Context context) {
        return forApp(context, EnumSet.noneOf(Flags.class));
    }

    /**
     * Returns an app-scoped SharedPreferences instance. You can disable
     * migrations by using the DISABLE_MIGRATIONS flag.
     */
    public static SharedPreferences forApp(Context context, EnumSet<Flags> flags) {
        if (flags != null && !flags.contains(Flags.DISABLE_MIGRATIONS)) {
            migrateIfNecessary(context);
        }

        return context.getSharedPreferences(APP_PREFS_NAME, 0);
    }

    public static SharedPreferences forCrashReporter(Context context) {
        return forCrashReporter(context, EnumSet.noneOf(Flags.class));
    }

    /**
     * Returns a crash-reporter-scoped SharedPreferences instance. You can disable
     * migrations by using the DISABLE_MIGRATIONS flag.
     */
    public static SharedPreferences forCrashReporter(Context context, EnumSet<Flags> flags) {
        if (flags != null && !flags.contains(Flags.DISABLE_MIGRATIONS)) {
            migrateIfNecessary(context);
        }

        return context.getSharedPreferences(CRASH_PREFS_NAME, 0);
    }

    public static SharedPreferences forProfile(Context context) {
        return forProfile(context, EnumSet.noneOf(Flags.class));
    }

    /**
     * Returns a SharedPreferences instance scoped to the current profile
     * in the app. You can disable migrations by using the DISABLE_MIGRATIONS
     * flag.
     */
    public static SharedPreferences forProfile(Context context, EnumSet<Flags> flags) {
        String profileName = GeckoProfile.get(context).getName();
        if (profileName == null) {
            throw new IllegalStateException("Could not get current profile name");
        }

        return forProfileName(context, profileName, flags);
    }

    public static SharedPreferences forProfileName(Context context, String profileName) {
        return forProfileName(context, profileName, EnumSet.noneOf(Flags.class));
    }

    /**
     * Returns an SharedPreferences instance scoped to the given profile name.
     * You can disable migrations by using the DISABLE_MIGRATION flag.
     */
    public static SharedPreferences forProfileName(Context context, String profileName,
            EnumSet<Flags> flags) {
        if (flags != null && !flags.contains(Flags.DISABLE_MIGRATIONS)) {
            migrateIfNecessary(context);
        }

        final String prefsName = PROFILE_PREFS_NAME_PREFIX + profileName;
        return context.getSharedPreferences(prefsName, 0);
    }

    /**
     * Returns the current version of the prefs.
     */
    public static int getVersion(Context context) {
        return forApp(context, disableMigrations).getInt(PREFS_VERSION_KEY, 0);
    }

    /**
     * Resets migration flag. Should only be used in tests.
     */
    public static synchronized void reset() {
        migrationDone = false;
    }

    /**
     * Performs all prefs migrations in the background thread to avoid StrictMode
     * exceptions from reading/writing in the UI thread. This method will block
     * the current thread until the migration is finished.
     */
    private static synchronized void migrateIfNecessary(final Context context) {
        if (migrationDone) {
            return;
        }

        // We deliberately perform the migration in the current thread (which
        // is likely the UI thread) as this is actually cheaper than enforcing a
        // context switch to another thread (see bug 940575).
        // Avoid strict mode warnings when doing so.
        final StrictMode.ThreadPolicy savedPolicy = StrictMode.allowThreadDiskReads();
        StrictMode.allowThreadDiskWrites();
        try {
            performMigration(context);
        } finally {
            StrictMode.setThreadPolicy(savedPolicy);
        }

        migrationDone = true;
    }

    private static void performMigration(Context context) {
        final SharedPreferences appPrefs = forApp(context, disableMigrations);

        final int currentVersion = appPrefs.getInt(PREFS_VERSION_KEY, 0);
        Log.d(LOGTAG, "Current version = " + currentVersion + ", prefs version = " + PREFS_VERSION);

        if (currentVersion == PREFS_VERSION) {
            return;
        }

        Log.d(LOGTAG, "Performing migration");

        final Editor appEditor = appPrefs.edit();

        // The migration always moves prefs to the default profile, not
        // the current one. We might have to revisit this if we ever support
        // multiple profiles.
        final String defaultProfileName;
        try {
            defaultProfileName = GeckoProfile.getDefaultProfileName(context);
        } catch (Exception e) {
            throw new IllegalStateException("Failed to get default profile name for migration");
        }

        final Editor profileEditor = forProfileName(context, defaultProfileName, disableMigrations).edit();
        final Editor crashEditor = forCrashReporter(context, disableMigrations).edit();

        List<String> profileKeys;
        Editor pmEditor = null;

        for (int v = currentVersion + 1; v <= PREFS_VERSION; v++) {
            Log.d(LOGTAG, "Migrating to version = " + v);

            switch (v) {
                case 1:
                    profileKeys = Arrays.asList(PROFILE_MIGRATIONS_0_TO_1);
                    pmEditor = migrateFromPreferenceManager(context, appEditor, profileEditor, profileKeys);
                    break;
                case 2:
                    profileKeys = Arrays.asList(PROFILE_MIGRATIONS_1_TO_2);
                    migrateCrashReporterSettings(appPrefs, appEditor, crashEditor, profileKeys);
                    break;
            }
        }

        // Update prefs version accordingly.
        appEditor.putInt(PREFS_VERSION_KEY, PREFS_VERSION);

        appEditor.apply();
        profileEditor.apply();
        crashEditor.apply();
        if (pmEditor != null) {
            pmEditor.apply();
        }

        Log.d(LOGTAG, "All keys have been migrated");
    }

    /**
     * Moves all preferences stored in PreferenceManager's default prefs
     * to either app or profile scopes. The profile-scoped keys are defined
     * in given profileKeys list, all other keys are moved to the app scope.
     */
    public static Editor migrateFromPreferenceManager(Context context, Editor appEditor,
            Editor profileEditor, List<String> profileKeys) {
        Log.d(LOGTAG, "Migrating from PreferenceManager");

        final SharedPreferences pmPrefs =
                PreferenceManager.getDefaultSharedPreferences(context);

        for (Map.Entry<String, ?> entry : pmPrefs.getAll().entrySet()) {
            final String key = entry.getKey();

            final Editor to;
            if (profileKeys.contains(key)) {
                to = profileEditor;
            } else {
                to = appEditor;
            }

            putEntry(to, key, entry.getValue());
        }

        // Clear PreferenceManager's prefs once we're done
        // and return the Editor to be committed.
        return pmPrefs.edit().clear();
    }

    /**
     * Moves the crash reporter's preferences from the app-wide prefs
     * into its own shared prefs to avoid cross-process pref accesses.
     */
    public static void migrateCrashReporterSettings(SharedPreferences appPrefs, Editor appEditor,
                                                    Editor crashEditor, List<String> profileKeys) {
        Log.d(LOGTAG, "Migrating crash reporter settings");

        for (Map.Entry<String, ?> entry : appPrefs.getAll().entrySet()) {
            final String key = entry.getKey();

            if (profileKeys.contains(key)) {
                putEntry(crashEditor, key, entry.getValue());
                appEditor.remove(key);
            }
        }
    }

    private static void putEntry(Editor to, String key, Object value) {
        Log.d(LOGTAG, "Migrating key = " + key + " with value = " + value);

        if (value instanceof String) {
            to.putString(key, (String) value);
        } else if (value instanceof Boolean) {
            to.putBoolean(key, (Boolean) value);
        } else if (value instanceof Long) {
            to.putLong(key, (Long) value);
        } else if (value instanceof Float) {
            to.putFloat(key, (Float) value);
        } else if (value instanceof Integer) {
            to.putInt(key, (Integer) value);
        } else {
            throw new IllegalStateException("Unrecognized value type for key: " + key);
        }
    }
}